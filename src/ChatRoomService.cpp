#include "../include/Logger.h"
#include "Metrics.h"
#include <iostream>
#include "../include/ChatRoomService.h"
#include "../include/Metrics.h"

#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include "../include/Persistence.h"
#include <fstream>
#include <sstream>

// forward factory
Persistence* CreateSqlitePersistence(const std::string& dbPath);

ChatRoomService::ChatRoomService(ChatRoomManager& roomManager, const Config& cfg)
    : roomManager_(roomManager), cfg_(cfg)
{
    if (!cfg_.dbPath.empty())
    {
        persistence_.reset(CreateSqlitePersistence(cfg_.dbPath));
        if (!persistence_)
        {
            // Failed to initialize persistence, leave as nullptr but log
            std::cerr << "Warning: failed to init persistence at " << cfg_.dbPath << std::endl;
        }
    }
}

bool ChatRoomService::sendErrorOrDisconnect(int clientFd, const SendMessageFn& sendMessage,
                                            const DisconnectFn& disconnectClient,
                                            ChatProtocolErrorCode code, const std::string& message,
                                            const std::string& disconnectReason)
{
    if (sendMessage(clientFd, encodeErrorResponse(code, message)))
    {
        return true;
    }

    disconnectClient(clientFd, disconnectReason);
    return false;
}

void ChatRoomService::pruneExpiredRateWindows(std::chrono::steady_clock::time_point now)
{
    const auto cleanupAge = std::chrono::minutes(1);

    for (auto it = userMsgWindow_.begin(); it != userMsgWindow_.end();)
    {
        if (now - it->second.second > cleanupAge)
        {
            it = userMsgWindow_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = roomMsgWindow_.begin(); it != roomMsgWindow_.end();)
    {
        if (now - it->second.second > cleanupAge)
        {
            it = roomMsgWindow_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ChatRoomService::processIncomingFrame(int clientFd, ChatSession& session, const std::string& frameBody,
                                           const SendMessageFn& sendMessage,
                                           const DisconnectFn& disconnectClient)
{
    ChatProtocolErrorCode parseErrorCode = ChatProtocolErrorCode::InvalidFormat;
    std::string parseErrorMessage;
    const std::optional<ChatProtocolMessage> message = parseChatProtocolMessage(frameBody, &parseErrorCode, &parseErrorMessage);
    if (!message)
    {
        sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, parseErrorCode, parseErrorMessage,
                              "output buffer full");
        return;
    }

    // Rate limiting checks (only for message frames)
    if (message->type == ChatMessageType::Message)
    {
        auto now = std::chrono::steady_clock::now();
        pruneExpiredRateWindows(now);

        // global window
        if (now - globalMsgWindow_.second >= std::chrono::seconds(1))
        {
            globalMsgWindow_.second = now;
            globalMsgWindow_.first = 0;
        }
        globalMsgWindow_.first++;
        if (globalMsgWindow_.first > cfg_.globalMsgPerSecond)
        {
            sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, ChatProtocolErrorCode::RateLimited,
                                  "global rate limit exceeded", "global rate limit");
            return;
        }

        // per-user
        const std::string userKey = session.authenticated ? session.username : ("anon#" + std::to_string(clientFd));
        auto uit = userMsgWindow_.find(userKey);
        if (uit == userMsgWindow_.end() || now - uit->second.second >= std::chrono::seconds(1))
        {
            userMsgWindow_[userKey] = {1, now};
        }
        else
        {
            uit->second.first++;
            if (uit->second.first > cfg_.userMsgPerSecond)
            {
                sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, ChatProtocolErrorCode::RateLimited,
                                      "user rate limit exceeded", "user rate limit");
                return;
            }
        }

        // per-room
        if (session.roomId != 0)
        {
            auto rit = roomMsgWindow_.find(session.roomId);
            if (rit == roomMsgWindow_.end() || now - rit->second.second >= std::chrono::seconds(1))
            {
                roomMsgWindow_[session.roomId] = {1, now};
            }
            else
            {
                rit->second.first++;
                if (rit->second.first > cfg_.roomMsgPerSecond)
                {
                    sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, ChatProtocolErrorCode::RateLimited,
                                          "room rate limit exceeded", "room rate limit");
                    return;
                }
            }
        }
    }

    // authentication handling
    bool requireAuth = !cfg_.authTokenToUser.empty();
    if (!session.joined)
    {
        if (!session.authenticated)
        {
            if (message->type == ChatMessageType::Auth)
            {
                const auto& auth = std::get<ChatAuthMessage>(message->payload);
                auto it = cfg_.authTokenToUser.find(auth.token);
                if (it == cfg_.authTokenToUser.end())
                {
                    sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, ChatProtocolErrorCode::InvalidField,
                                          "invalid auth token", "output buffer full");
                    return;
                }

                session.authenticated = true;
                session.authToken = auth.token;
                session.username = it->second;

                // send auth ack
                ChatProtocolMessage ack = ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::AuthAck,
                                                              ChatProtocolPayload{ChatAuthAckMessage{session.username}}};
                if (!sendMessage(clientFd, encodeChatProtocolMessage(ack)))
                {
                    disconnectClient(clientFd, "output buffer full");
                }
                return;
            }

            if (requireAuth)
            {
                sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, ChatProtocolErrorCode::InvalidField,
                                      "authentication required", "output buffer full");
                return;
            }
        }

        // legacy or post-auth join
        if (message->type == ChatMessageType::Join)
        {
            const auto& joinMessage = std::get<ChatJoinMessage>(message->payload);
            const std::string username = session.authenticated ? session.username : joinMessage.username;

            // persist membership if persistence configured
            if (persistence_)
            {
                persistence_->addMembership(joinMessage.roomId, username);
            }

            handleJoin(clientFd, session, JoinInfo{joinMessage.roomId, username, {}}, sendMessage,
                       disconnectClient);
            return;
        }

        if (message->type == ChatMessageType::Message)
        {
            JoinInfo joinInfo;
            joinInfo.roomId = 0;
            joinInfo.username = session.authenticated ? session.username : ("user" + std::to_string(clientFd));
            joinInfo.pendingMessage = std::get<ChatTextMessage>(message->payload).text;
            handleJoin(clientFd, session, joinInfo, sendMessage, disconnectClient);
            return;
        }

        if (message->type == ChatMessageType::Heartbeat || message->type == ChatMessageType::ErrorResponse)
        {
            return;
        }

        return;
    }

    if (message->type == ChatMessageType::Heartbeat)
    {
        return;
    }

    if (message->type == ChatMessageType::ErrorResponse)
    {
        return;
    }

    if (message->type == ChatMessageType::Join)
    {
        sendErrorOrDisconnect(clientFd, sendMessage, disconnectClient, ChatProtocolErrorCode::InvalidField,
                              "client is already joined", "output buffer full");
        return;
    }

    const auto& textMessage = std::get<ChatTextMessage>(message->payload);
    // append audit
    long long ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (persistence_)
    {
        persistence_->appendAudit(session.roomId, session.username, textMessage.text, ts);
    }

    Metrics::instance().incMessagesReceived();

    // deliver to live users
    broadcastToRoom(clientFd, session, "Client[" + session.username + "]: " + textMessage.text, sendMessage,
                    disconnectClient);
    Metrics::instance().incMessagesBroadcasted();

    // store offline messages for persisted members who are not currently connected
    if (persistence_)
    {
        auto members = persistence_->getRoomMembers(session.roomId);
        auto live = session.room ? session.room->snapshotClients() : std::vector<User::Ptr>{};
        std::unordered_map<std::string, bool> liveMap;
        for (const auto& u : live)
        {
            if (u) liveMap[u->username()] = true;
        }
        for (const auto& member : members)
        {
            if (member.empty()) continue;
            if (liveMap.find(member) == liveMap.end())
            {
                persistence_->storeOfflineMessage(session.roomId, member, session.username, textMessage.text, ts);
            }
        }
    }
}

void ChatRoomService::detachClient(int clientFd, ChatSession& session, const SendMessageFn& sendMessage,
                                   const DisconnectFn& disconnectClient)
{
    if (!session.joined || !session.room || !session.user)
    {
        return;
    }

    broadcastToRoom(clientFd, session, "[server] " + session.username + " left: " + std::to_string(clientFd),
                    sendMessage, disconnectClient);
    session.room->removeClient(session.user);
    session.joined = false;
    session.roomId = 0;
    session.username.clear();
    session.room.reset();
    session.user.reset();
}

void ChatRoomService::handleJoin(int clientFd, ChatSession& session, const JoinInfo& joinInfo,
                                 const SendMessageFn& sendMessage,
                                 const DisconnectFn& disconnectClient)
{
    session.roomId = joinInfo.roomId;
    session.username = joinInfo.username;
    session.room = roomManager_.getOrCreateRoom(joinInfo.roomId);
    session.user = std::make_shared<User>(clientFd, session.username, session.room.get());
    session.joined = true;

    session.room->addClient(session.user);

    if (!sendMessage(clientFd, "Welcome to room " + std::to_string(session.roomId) + "!"))
    {
        disconnectClient(clientFd, "output buffer full");
        return;
    }

    broadcastToRoom(clientFd, session, "[server] " + session.username + " connected to room " + std::to_string(session.roomId),
                    sendMessage, disconnectClient);

    if (!joinInfo.pendingMessage.empty())
    {
        broadcastToRoom(clientFd, session, "Client[" + session.username + "]: " + joinInfo.pendingMessage,
                        sendMessage, disconnectClient);
    }
}

    void ChatRoomService::broadcastToRoom(int senderFd, const ChatSession& session, const std::string& message,
                                      const SendMessageFn& sendMessage,
                                      const DisconnectFn& disconnectClient)
{
    if (!session.room)
    {
        return;
    }

    const uint32_t senderId = session.user ? session.user->id() : static_cast<uint32_t>(-1);
    auto users = session.room->snapshotClients();
    // enforce fanout limit
    int delivered = 0;
    int fanoutLimit = cfg_.maxBroadcastFanout > 0 ? cfg_.maxBroadcastFanout : static_cast<int>(users.size());
    for (const auto& user : users)
    {
        if (!user || user->id() == senderId)
        {
            continue;
        }

        if (delivered >= fanoutLimit) break;

        if (!sendMessage(static_cast<int>(user->id()), message))
        {
            Metrics::instance().incBroadcastFailures();
            if (cfg_.rejectStrategy == "disconnect")
            {
                disconnectClient(static_cast<int>(user->id()), "output buffer full");
            }
            else if (cfg_.rejectStrategy == "reject")
            {
                if (!sendMessage(senderFd, encodeErrorResponse(ChatProtocolErrorCode::RateLimited,
                                                              "broadcast rejected due to slow recipient")))
                {
                    disconnectClient(senderFd, "output buffer full");
                }
                break;
            }
        }
        else
        {
            delivered++;
        }
    }
}

void ChatRoomService::persistState()
{
    if (!persistence_)
    {
        // no DB persistence configured; write a JSON snapshot file next to configured DB path if available
        if (cfg_.dbPath.empty()) return;
        std::string snapshotPath = cfg_.dbPath + ".snapshot.json";
        std::ofstream out(snapshotPath, std::ios::trunc);
        if (!out) return;
        out << "{";
        out << "\"rooms\": [";
        auto rooms = roomManager_.getAllRooms();
        bool firstRoom = true;
        for (const auto& room : rooms)
        {
            if (!firstRoom) out << ",";
            firstRoom = false;
            out << "{\"roomId\":" << room->roomId() << ",\"members\": [";
            auto members = room->snapshotClients();
            bool first = true;
            for (const auto& u : members)
            {
                if (!u) continue;
                if (!first) out << ",";
                first = false;
                out << "\"" << u->username() << "\"";
            }
            out << "]}";
        }
        out << "]}";
        out.close();
        Logger::info("persist_snapshot", "\"path\":\"" + snapshotPath + "\"");
        return;
    }

    // Persist memberships into the configured persistence backend
    auto rooms = roomManager_.getAllRooms();
    for (const auto& room : rooms)
    {
        auto members = room->snapshotClients();
        for (const auto& u : members)
        {
            if (!u) continue;
            persistence_->addMembership(static_cast<int>(room->roomId()), u->username());
        }
    }
    Logger::info("persist_db", "\"rooms\":" + std::to_string(rooms.size()));
}