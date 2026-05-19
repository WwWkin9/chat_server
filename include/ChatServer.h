#pragma once

#include <sys/epoll.h>
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
    struct ClientState
    {
        int fd = -1;
        std::string inputBuffer;
        std::string outputBuffer;
        std::chrono::steady_clock::time_point lastActivity;
        ChatSession session;
        // rate limiting window for incoming messages
        std::chrono::steady_clock::time_point msgWindowStart = std::chrono::steady_clock::now();
        int msgsInWindow = 0;
    };

    bool setupListenSocket();
    bool setNonblocking(int fd);
    void acceptNewClients();
    void handleReadableClients(int fd);
    void handleWritableClients(int fd);
    void markClientActive(int fd);
    void reapIdleClients();
    bool enqueueMessage(int fd, const std::string& message);
    bool updateClientWriteInterest(int fd, bool enableWrite);
    bool registerClient(int fd);
    void disconnectClient(int fd, const std::string& reason);
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
    size_t maxInputFrameBytes_ = 16 * 1024;
    int connMsgPerSecond_ = 20;
    std::chrono::seconds clientIdleTimeout_{std::chrono::minutes(15)};
    int backlog_ = 128;
    int roomLimit_ = 1000;
    std::string logLevel_;
    std::string tlsCertPath_;
    std::string tlsKeyPath_;
    ChatRoomManager roomManager_;
    ChatRoomService roomService_;
    Config cfg_;
    // reactor pool and fd->reactor mapping
    std::vector<std::unique_ptr<WorkerReactor>> reactors_;
    std::unordered_map<int, int> fdToReactor_; // fd -> reactor index
    std::mutex fdMapMtx_;
    // global rate window
    std::chrono::steady_clock::time_point globalMsgWindowStart_ = std::chrono::steady_clock::now();
    int globalMsgsInWindow_ = 0;
    size_t reactorCount_ = 0;
    size_t nextReactorIdx_ = 0;
};