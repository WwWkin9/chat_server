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
    };

    using MigrateFn = std::function<void(std::size_t, ClientState)>;

    WorkerReactor(int id, ChatRoomService& svc, SendFn router, DisconnectFn disrouter,
                  RoomAffinityFn roomAffinityFn, MigrateFn migrateFn,
                  int maxOutputBytes, int idleSeconds);
    ~WorkerReactor();

    void start();
    void stop();

    // 由 acceptor 线程调用
    void addClientFd(int fd);
    // 线程安全地为本 reactor 所属的 fd 入队消息
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

    // 来自其他线程的待处理操作
    std::mutex pendingMtx_;
    std::vector<int> pendingNewFds_;
    std::unordered_map<int, std::vector<std::string>> pendingMsgs_;
    std::vector<int> pendingDisconnects_;
    std::vector<ClientState> pendingAdoptions_;

    // 每个 reactor 自己维护的客户端，只能由 reactor 线程访问
    std::unordered_map<int, ClientState> clients_;
    int epollFd_ = -1;
    int eventFd_ = -1;
};
