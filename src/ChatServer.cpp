#include "../include/ChatServer.h"
#include "../include/SocketFrameIO.h"
#include "../include/WorkerReactor.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <thread>
#include <atomic>
#include "../include/Metrics.h"
#include "../include/Logger.h"
#include <sstream>

ChatServer::ChatServer(const Config& cfg)
        : port_(cfg.port), maxOutputBufferBytes_(cfg.maxOutputBufferBytes),
            clientIdleTimeout_{std::chrono::seconds(cfg.clientIdleTimeoutSeconds)},
            backlog_{cfg.backlog}, roomManager_(cfg.roomShardCount), roomService_(roomManager_, cfg),
            cfg_(cfg)
{
}

namespace
{
constexpr auto DEFAULT_CLIENT_IDLE_TIMEOUT = std::chrono::minutes(15);

volatile sig_atomic_t g_stopRequested = 0;

void handleSignal(int)
{
    g_stopRequested = 1;
}
}

bool ChatServer::run()
{
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (clientIdleTimeout_ == std::chrono::seconds::zero())
    {
        clientIdleTimeout_ = std::chrono::duration_cast<std::chrono::seconds>(DEFAULT_CLIENT_IDLE_TIMEOUT);
    }


    if (!setupListenSocket())
    {
        cleanup();
        return false;
    }

    // 创建 worker reactor
    reactorCount_ = cfg_.reactorThreads > 0 ? cfg_.reactorThreads : std::max(1u, std::thread::hardware_concurrency());
    for (size_t i = 0; i < reactorCount_; ++i)
    {
        // 路由回调会返回到这个 ChatServer，由它转发到对应的 reactor
        auto router = [this](int targetFd, const std::string& message) -> bool { return this->routeSendMessage(targetFd, message); };
        auto disrouter = [this](int targetFd, const std::string& reason) { this->routeDisconnect(targetFd, reason); };
        auto affinity = [this](int roomId) -> std::size_t { return this->roomAffinityIndex(roomId); };
        auto migrate = [this](std::size_t targetIdx, WorkerReactor::ClientState state) { this->migrateClientToReactor(targetIdx, std::move(state)); };
        reactors_.push_back(std::make_unique<WorkerReactor>(static_cast<int>(i), roomService_, router, disrouter, affinity, migrate, static_cast<int>(maxOutputBufferBytes_), static_cast<int>(clientIdleTimeout_.count())));
        reactors_.back()->start();
    }

    std::cout << "Chat server started on port " << port_ << std::endl;
    Logger::info("server_start", "\"port\":" + std::to_string(port_));

    // 在 port+1 启动健康检查 HTTP 服务
    std::atomic<bool> healthRunning{true};
    std::thread healthThread([this, &healthRunning]() {
        int hp = port_ + 1;
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return;
        int opt = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in haddr{}; haddr.sin_family = AF_INET; haddr.sin_port = htons(hp); haddr.sin_addr.s_addr = INADDR_ANY;
        if (bind(s, (sockaddr*)&haddr, sizeof(haddr)) < 0) { close(s); return; }
        if (listen(s, 5) < 0) { close(s); return; }
        if (!setNonblocking(s)) { close(s); return; }
        Logger::info("health_listen", "\"port\":" + std::to_string(hp));
        while (healthRunning.load())
        {
            int c = accept(s, nullptr, nullptr);
            if (c < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            std::string body = Metrics::instance().toJson();
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << body.size() << "\r\n\r\n" << body;
            std::string out = resp.str();
            send(c, out.data(), out.size(), 0);
            close(c);
        }
        close(s);
    });

    // 接收循环：接收新连接并分发给 worker reactor
    while (!g_stopRequested)
    {
        acceptNewClients();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "Shutdown requested, entering graceful shutdown" << std::endl;
    Logger::info("shutdown_requested", "");

    // 停止接收新连接
    if (listenFd_ >= 0)
    {
        close(listenFd_);
        listenFd_ = -1;
        Logger::info("stop_accepting", "");
    }

    // 计算优雅退出的截止时间
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(cfg_.gracefulShutdownSeconds);

    // 通知各 reactor 停止，并允许它们在截止时间前完成收尾
    for (auto &r : reactors_)
    {
        if (r) r->stop(); // stop will join thread after wakeup
    }

    // 如果启用了持久化，则保存状态
    if (cfg_.persistOnShutdown)
    {
        Logger::info("persisting_state", "");
        roomService_.persistState();
    }

    Logger::info("graceful_shutdown_complete", "");
    healthRunning = false;
    if (healthThread.joinable())
    {
        healthThread.join();
    }
    cleanup();
    Logger::info("server_stop", "");
    return true;
}

bool ChatServer::setupListenSocket()
{
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Failed to bind socket: " << std::strerror(errno) << std::endl;
        close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    if (listen(listenFd_, backlog_) < 0)
    {
        std::cerr << "Failed to listen on socket: " << std::strerror(errno) << std::endl;
        close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    return setNonblocking(listenFd_);
}

bool ChatServer::setNonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "fcntl F_GETFL error: " << std::strerror(errno) << std::endl;
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "fcntl F_SETFL error: " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

void ChatServer::acceptNewClients()
{
    while (true)
    {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientFd = accept(listenFd_, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientFd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            std::cerr << "Failed to accept client connection" << std::endl;
            break;
        }

        if (!setNonblocking(clientFd))
        {
            close(clientFd);
            continue;
        }
        // assign to a worker reactor
        if (reactors_.empty())
        {
            // no reactors: close
            close(clientFd);
            continue;
        }
        size_t idx = nextReactorIdx_++ % reactorCount_;
        {
            std::lock_guard<std::mutex> lk(fdMapMtx_);
            fdToReactor_[clientFd] = static_cast<int>(idx);
        }
        reactors_[idx]->addClientFd(clientFd);
        Logger::info("new_client", "\"fd\":" + std::to_string(clientFd) + ",\"reactor\":" + std::to_string(idx));
    }
}

bool ChatServer::routeSendMessage(int fd, const std::string& message)
{
    int idx = -1;
    {
        std::lock_guard<std::mutex> lk(fdMapMtx_);
        auto it = fdToReactor_.find(fd);
        if (it == fdToReactor_.end()) return false;
        idx = it->second;
    }
    if (idx < 0 || static_cast<size_t>(idx) >= reactors_.size()) return false;
    return reactors_[idx]->enqueueMessage(fd, message);
}

std::size_t ChatServer::roomAffinityIndex(int roomId) const
{
    if (reactorCount_ == 0)
    {
        return 0;
    }
    return static_cast<std::size_t>(roomId) % reactorCount_;
}

void ChatServer::migrateClientToReactor(std::size_t targetIdx, WorkerReactor::ClientState state)
{
    if (state.fd < 0 || targetIdx >= reactors_.size())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lk(fdMapMtx_);
        fdToReactor_[state.fd] = static_cast<int>(targetIdx);
    }

    reactors_[targetIdx]->adoptClientState(std::move(state));
}

void ChatServer::routeDisconnect(int fd, const std::string& reason)
{
    int idx = -1;
    {
        std::lock_guard<std::mutex> lk(fdMapMtx_);
        auto it = fdToReactor_.find(fd);
        if (it == fdToReactor_.end()) return;
        idx = it->second;
        fdToReactor_.erase(it);
    }
    if (idx < 0 || static_cast<size_t>(idx) >= reactors_.size()) return;
    reactors_[idx]->disconnectClient(fd, reason);
}

void ChatServer::cleanup()
{
    // 停止各个 reactor
    for (auto &r : reactors_)
    {
        if (r) r->stop();
    }
    reactors_.clear();

    // 关闭监听 socket
    if (listenFd_ >= 0)
    {
        close(listenFd_);
        listenFd_ = -1;
    }

    // 清空 fd 到 reactor 的映射
    {
        std::lock_guard<std::mutex> lk(fdMapMtx_);
        fdToReactor_.clear();
    }
}