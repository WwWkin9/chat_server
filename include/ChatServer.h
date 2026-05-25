#pragma once

#include "WorkerReactor.h"

#include "ChatRoomService.h"
#include "Config.h"

#include <cstddef>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

class ChatServer
{
public:
    explicit ChatServer(const Config& cfg = Config());
    bool run();

private:
    bool setupListenSocket();
    bool setNonblocking(int fd);
    void acceptNewClients();
    void cleanup();

    // routing helpers for multi-reactor: route send/disconnect to the owning reactor
    bool routeSendMessage(int fd, const std::string& message);
    void routeDisconnect(int fd, const std::string& reason);
    std::size_t roomAffinityIndex(int roomId) const;
    void migrateClientToReactor(std::size_t targetIdx, WorkerReactor::ClientState state);

    int port_;
    int listenFd_ = -1;
    // Each worker reactor manages its own epoll instance.
    size_t maxOutputBufferBytes_;// 每个客户端的输出缓冲区最大字节数
    std::chrono::seconds clientIdleTimeout_{std::chrono::minutes(15)};
    int backlog_ = 128;
    ChatRoomManager roomManager_;
    ChatRoomService roomService_;
    Config cfg_;
    // reactor pool and fd->reactor mapping
    std::vector<std::unique_ptr<WorkerReactor>> reactors_;
    std::unordered_map<int, int> fdToReactor_; // fd -> reactor index
    std::mutex fdMapMtx_;
    size_t reactorCount_ = 0;
    size_t nextReactorIdx_ = 0;
};