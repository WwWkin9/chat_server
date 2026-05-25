#include "../include/WorkerReactor.h"
#include "../include/SocketFrameIO.h"
#include "../include/Logger.h"
#include "../include/Metrics.h"
#include "../include/ChatRoomService.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

static int setNonblockingFd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

WorkerReactor::WorkerReactor(int id, ChatRoomService& svc, SendFn router, DisconnectFn disrouter,
                                                         RoomAffinityFn roomAffinityFn, MigrateFn migrateFn,
                                                         int maxOutputBytes, int idleSeconds)
        : id_(id), roomService_(svc), router_(std::move(router)), disrouter_(std::move(disrouter)),
            roomAffinityFn_(std::move(roomAffinityFn)), migrateFn_(std::move(migrateFn)),
            maxOutputBytes_(maxOutputBytes), idleSeconds_(idleSeconds)
{
}

WorkerReactor::~WorkerReactor()
{
    stop();
}

void WorkerReactor::start()
{
    if (running_.exchange(true)) return;
    // 创建 epoll 和 eventfd
    epollFd_ = epoll_create1(0);
    eventFd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (epollFd_ < 0 || eventFd_ < 0)
    {
        std::cerr << "WorkerReactor " << id_ << " failed to create epoll/eventfd" << std::endl;
        running_ = false;
        return;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = eventFd_;
    epoll_ctl(epollFd_, EPOLL_CTL_ADD, eventFd_, &ev);

    thread_ = std::thread(&WorkerReactor::run, this);
}

void WorkerReactor::stop()
{
    if (!running_.exchange(false)) return;
    // 唤醒线程
    uint64_t one = 1;
    if (eventFd_ >= 0) write(eventFd_, &one, sizeof(one));
    if (thread_.joinable()) thread_.join();
    if (eventFd_ >= 0) { close(eventFd_); eventFd_ = -1; }
    if (epollFd_ >= 0) { close(epollFd_); epollFd_ = -1; }
}

void WorkerReactor::addClientFd(int fd)
{
    std::lock_guard<std::mutex> lk(pendingMtx_);
    pendingNewFds_.push_back(fd);
    uint64_t one = 1;
    if (eventFd_ >= 0) write(eventFd_, &one, sizeof(one));
}

bool WorkerReactor::enqueueMessage(int fd, const std::string& message)
{
    std::lock_guard<std::mutex> lk(pendingMtx_);
    pendingMsgs_[fd].push_back(message);
    uint64_t one = 1;
    if (eventFd_ >= 0) write(eventFd_, &one, sizeof(one));
    return true;
}

void WorkerReactor::disconnectClient(int fd, const std::string& reason)
{
    std::lock_guard<std::mutex> lk(pendingMtx_);
    pendingDisconnects_.push_back(fd);
    uint64_t one = 1;
    if (eventFd_ >= 0) write(eventFd_, &one, sizeof(one));
}

void WorkerReactor::adoptClientState(ClientState state)
{
    std::lock_guard<std::mutex> lk(pendingMtx_);
    pendingAdoptions_.push_back(std::move(state));
    uint64_t one = 1;
    if (eventFd_ >= 0) write(eventFd_, &one, sizeof(one));
}

void WorkerReactor::processPending()
{
    std::vector<int> newFds;
    std::unordered_map<int, std::vector<std::string>> msgs;
    std::vector<int> disconnects;
    std::vector<ClientState> adoptions;

    {
        std::lock_guard<std::mutex> lk(pendingMtx_);
        newFds.swap(pendingNewFds_);
        msgs.swap(pendingMsgs_);
        disconnects.swap(pendingDisconnects_);
        adoptions.swap(pendingAdoptions_);
    }

    for (int fd : newFds)
    {
        if (setNonblockingFd(fd) != 0)
        {
            close(fd);
            continue;
        }
        ClientState st;
        st.fd = fd;
        st.lastActivity = std::chrono::steady_clock::now();
        clients_[fd] = std::move(st);

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
        Metrics::instance().incAccepted();
        Logger::info("worker_new_client", "\"reactor\":" + std::to_string(id_) + ",\"fd\":" + std::to_string(fd));
    }

    for (auto &p : msgs)
    {
        int fd = p.first;
        auto it = clients_.find(fd);
        if (it == clients_.end()) continue;
        bool wasEmpty = it->second.outputBuffer.empty();
        bool overflowed = false;
        for (auto &m : p.second)
        {
            const std::size_t frameSize = sizeof(std::uint32_t) + m.size();
            if (it->second.outputBuffer.size() + frameSize > static_cast<std::size_t>(maxOutputBytes_))
            {
                overflowed = true;
                break;
            }
            queueFrame(it->second.outputBuffer, m);
        }

        if (overflowed)
        {
            roomService_.detachClient(fd, it->second.session,
                                      [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                      [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
            close(fd);
            clients_.erase(fd);
            Metrics::instance().incDisconnected("output buffer full");
            Logger::warn("worker_output_overflow", "\"reactor\":" + std::to_string(id_) + ",\"fd\":" + std::to_string(fd));
            continue;
        }

        Metrics::instance().updateQueueDepth(fd, it->second.outputBuffer.size());
        if (wasEmpty && !it->second.outputBuffer.empty())
        {
            epoll_event ev{}; ev.events = EPOLLIN | EPOLLET | EPOLLOUT; ev.data.fd = fd;
            epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
        }
    }

    for (int fd : disconnects)
    {
        auto it = clients_.find(fd);
        if (it == clients_.end()) continue;
        roomService_.detachClient(fd, it->second.session,
                                  [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                  [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
        close(fd);
        clients_.erase(fd);
        Metrics::instance().incDisconnected("remote");
    }

    for (auto& state : adoptions)
    {
        int fd = state.fd;
        if (fd < 0)
        {
            continue;
        }

        clients_[fd] = std::move(state);
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        if (!clients_[fd].outputBuffer.empty())
        {
            ev.events |= EPOLLOUT;
        }
        ev.data.fd = fd;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
        Logger::info("worker_adopt_client", "\"reactor\":" + std::to_string(id_) + ",\"fd\":" + std::to_string(fd));
    }
}

void WorkerReactor::run()
{
    const int MAX_EVENTS = 256;
    std::vector<epoll_event> events(MAX_EVENTS);
    while (running_)
    {
        int n = epoll_wait(epollFd_, events.data(), MAX_EVENTS, 1000);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            break;
        }

        // 先处理唤醒和待处理操作
        processPending();

        for (int i = 0; i < n; ++i)
        {
            epoll_event &e = events[i];
            int fd = e.data.fd;
            if (fd == eventFd_)
            {
                uint64_t v; read(eventFd_, &v, sizeof(v));
                continue;
            }

            auto it = clients_.find(fd);
            if (it == clients_.end()) continue;

            if (e.events & EPOLLIN)
            {
                auto &client = it->second;
                const bool wasJoined = client.session.joined;
                client.lastActivity = std::chrono::steady_clock::now();
                if (!receiveIntoBuffer(fd, client.inputBuffer))
                {
                    // 断开连接
                    roomService_.detachClient(fd, client.session,
                                              [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                              [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
                    close(fd);
                    clients_.erase(fd);
                    Metrics::instance().incDisconnected("read_fail");
                    continue;
                }

                std::string body;
                while (popFrame(client.inputBuffer, body))
                {
                        if (body.size() > 16*1024) // 基本限制
                    {
                        router_(fd, "ERR|frame too large");
                        roomService_.detachClient(fd, client.session,
                                                  [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                                  [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
                        close(fd);
                        clients_.erase(fd);
                        break;
                    }

                    roomService_.processIncomingFrame(fd, client.session, body,
                                                      [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                                      [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
                }

                if (!wasJoined && client.session.joined)
                {
                    const std::size_t targetReactor = roomAffinityFn_ ? roomAffinityFn_(client.session.roomId) : static_cast<std::size_t>(id_);
                    if (targetReactor != static_cast<std::size_t>(id_) && migrateFn_)
                    {
                        ClientState snapshot = client;
                        snapshot.fd = fd;
                        migrateFn_(targetReactor, std::move(snapshot));

                        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
                        clients_.erase(fd);
                        Logger::info("worker_migrate_client", "\"from\":" + std::to_string(id_) + ",\"to\":" + std::to_string(targetReactor) + ",\"fd\":" + std::to_string(fd));
                        continue;
                    }
                }
            }

            if (e.events & EPOLLOUT)
            {
                auto &client = it->second;
                if (!flushOutput(fd, client.outputBuffer))
                {
                    roomService_.detachClient(fd, client.session,
                                              [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                              [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
                    close(fd);
                    clients_.erase(fd);
                    Metrics::instance().incDisconnected("write_fail");
                    continue;
                }

                if (client.outputBuffer.empty())
                {
                    epoll_event ev{}; ev.events = EPOLLIN | EPOLLET; ev.data.fd = fd;
                    epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
                }
            }
        }

        // 周期性回收空闲连接
        auto now = std::chrono::steady_clock::now();
        std::vector<int> toDisconnect;
        for (auto &p : clients_)
        {
            if (now - p.second.lastActivity > std::chrono::seconds(idleSeconds_))
            {
                toDisconnect.push_back(p.first);
            }
        }
        for (int fd : toDisconnect)
        {
            auto it2 = clients_.find(fd);
            if (it2 == clients_.end()) continue;
            roomService_.detachClient(fd, it2->second.session,
                                      [this](int targetFd, const std::string& message) { return router_(targetFd, message); },
                                      [this](int targetFd, const std::string& reason) { disrouter_(targetFd, reason); });
            close(fd);
            clients_.erase(fd);
            Metrics::instance().incDisconnected("idle");
        }
    }
}
