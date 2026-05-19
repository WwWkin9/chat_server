#pragma once

#include <atomic>
#include <cstddef>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ChatSession.h"

class ChatRoomService;

class WorkerReactor
{
public:
    using SendFn = std::function<bool(int, const std::string&)>;
    using DisconnectFn = std::function<void(int, const std::string&)>;
    using RoomAffinityFn = std::function<std::size_t(int)>;

    struct ClientState
    {
        int fd = -1;
        std::string inputBuffer;
        std::string outputBuffer;
        std::chrono::steady_clock::time_point lastActivity;
        ChatSession session;
        std::chrono::steady_clock::time_point msgWindowStart = std::chrono::steady_clock::now();
        int msgsInWindow = 0;
    };

    using MigrateFn = std::function<void(std::size_t, ClientState)>;

    WorkerReactor(int id, ChatRoomService& svc, SendFn router, DisconnectFn disrouter,
                  RoomAffinityFn roomAffinityFn, MigrateFn migrateFn,
                  int maxOutputBytes, int idleSeconds);
    ~WorkerReactor();

    void start();
    void stop();

    // called from acceptor thread
    void addClientFd(int fd);
    // thread-safe enqueue message for fd owned by this reactor
    bool enqueueMessage(int fd, const std::string& message);
    void disconnectClient(int fd, const std::string& reason);
    void adoptClientState(ClientState state);

    int id() const { return id_; }

private:
    void run();
    void processPending();

    int id_;
    ChatRoomService& roomService_;
    SendFn router_; // routes send to owning reactor (may call back to other reactors)
    DisconnectFn disrouter_;
    RoomAffinityFn roomAffinityFn_;
    MigrateFn migrateFn_;
    int maxOutputBytes_;
    int idleSeconds_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    // pending operations from other threads
    std::mutex pendingMtx_;
    std::vector<int> pendingNewFds_;
    std::unordered_map<int, std::vector<std::string>> pendingMsgs_;
    std::vector<int> pendingDisconnects_;
    std::vector<ClientState> pendingAdoptions_;

    // per-reactor clients, only accessed by reactor thread
    std::unordered_map<int, ClientState> clients_;
    int epollFd_ = -1;
    int eventFd_ = -1;
};
