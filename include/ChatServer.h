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

    // 多 reactor 的路由辅助：把发送/断开请求转发给拥有该 fd 的 reactor
    bool routeSendMessage(int fd, const std::string& message);
    void routeDisconnect(int fd, const std::string& reason);
    std::size_t roomAffinityIndex(int roomId) const;
    void migrateClientToReactor(std::size_t targetIdx, WorkerReactor::ClientState state);

    int port_;
    int listenFd_ = -1;
    // 每个 worker reactor 维护自己的 epoll 实例。
    size_t maxOutputBufferBytes_;// 每个客户端的输出缓冲区最大字节数
    std::chrono::seconds clientIdleTimeout_{std::chrono::minutes(15)};
    int backlog_ = 128;
    ChatRoomManager roomManager_;
    ChatRoomService roomService_;
    Config cfg_;
    // reactor 池和 fd 到 reactor 的映射
    std::vector<std::unique_ptr<WorkerReactor>> reactors_;
    std::unordered_map<int, int> fdToReactor_; // fd -> reactor 索引
    std::mutex fdMapMtx_;
    size_t reactorCount_ = 0;
    size_t nextReactorIdx_ = 0;
};