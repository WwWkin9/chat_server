#pragma once

#include "ChatProtocol.h"
#include "Config.h"
#include "ChatRoomManager.h"
#include "ChatSession.h"
#include "Persistence.h"

#include <functional>
#include <chrono>
#include <memory>
#include <unordered_map>

class ChatRoomService
{
public:
    using SendMessageFn = std::function<bool(int, const std::string&)>;
    using DisconnectFn = std::function<void(int, const std::string&)>;

    explicit ChatRoomService(ChatRoomManager& roomManager, const Config& cfg);

    void processIncomingFrame(int clientFd, ChatSession& session, const std::string& frameBody,
                              const SendMessageFn& sendMessage,
                              const DisconnectFn& disconnectClient);
    void detachClient(int clientFd, ChatSession& session, const SendMessageFn& sendMessage,
                      const DisconnectFn& disconnectClient);

private:
    bool sendErrorOrDisconnect(int clientFd, const SendMessageFn& sendMessage,
                                const DisconnectFn& disconnectClient,
                                ChatProtocolErrorCode code, const std::string& message,
                                const std::string& disconnectReason);
    void pruneExpiredRateWindows(std::chrono::steady_clock::time_point now);
    void handleJoin(int clientFd, ChatSession& session, const JoinInfo& joinInfo,
                    const SendMessageFn& sendMessage,
                    const DisconnectFn& disconnectClient);
    void broadcastToRoom(int senderFd, const ChatSession& session, const std::string& message,
                         const SendMessageFn& sendMessage,
                         const DisconnectFn& disconnectClient);

    ChatRoomManager& roomManager_;
    const Config& cfg_;
    std::unique_ptr<Persistence> persistence_;
    // rate windows
    std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> userMsgWindow_;
    std::unordered_map<int, std::pair<int, std::chrono::steady_clock::time_point>> roomMsgWindow_;
    std::pair<int, std::chrono::steady_clock::time_point> globalMsgWindow_{0, std::chrono::steady_clock::now()};
public:
    // persist in-memory state to persistent storage (DB or snapshot file)
    void persistState();
};