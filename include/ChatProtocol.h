#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

constexpr std::uint16_t kChatProtocolVersion = 1;
constexpr std::size_t kChatProtocolMaxFrameBytes = 1024;
constexpr std::size_t kChatProtocolMaxUsernameBytes = 32;
constexpr std::size_t kChatProtocolMaxPasswordBytes = 128;
constexpr std::size_t kChatProtocolMaxMessageBytes = 512;
constexpr std::size_t kChatProtocolMaxErrorMessageBytes = 128;

enum class ChatMessageType
{
    Join,
    Message,
    Auth,
    Register,
    Login,
    AuthAck,
    Heartbeat,
    ErrorResponse
};

enum class ChatProtocolErrorCode : std::uint16_t
{
    UnsupportedVersion = 1,
    InvalidEncoding = 2,
    InvalidFormat = 3,
    InvalidField = 4,
    FieldTooLong = 5,
    MessageTooLong = 6,
    RateLimited = 7
};

struct ChatJoinMessage
{
    int roomId = 0;
    std::string username;
};

struct ChatTextMessage
{
    std::string text;
};

struct ChatHeartbeatMessage
{
};

struct ChatAuthMessage
{
    std::string token;
};

struct ChatCredentialMessage
{
    std::string username;
    std::string password;
};

struct ChatAuthAckMessage
{
    std::string username;
};

struct ChatErrorResponse
{
    ChatProtocolErrorCode code = ChatProtocolErrorCode::InvalidFormat;
    std::string message;
};

using ChatProtocolPayload = std::variant<ChatHeartbeatMessage, ChatJoinMessage, ChatTextMessage, ChatErrorResponse,
                                         ChatAuthMessage, ChatCredentialMessage, ChatAuthAckMessage>;

struct ChatProtocolMessage
{
    std::uint16_t version = kChatProtocolVersion;
    ChatMessageType type = ChatMessageType::Heartbeat;
    ChatProtocolPayload payload = ChatHeartbeatMessage{};
};

struct JoinInfo
{
    int roomId = 0;
    std::string username;
    std::string pendingMessage;
};

std::optional<ChatProtocolMessage> parseChatProtocolMessage(const std::string& frameBody,
                                                           ChatProtocolErrorCode* errorCode = nullptr,
                                                           std::string* errorMessage = nullptr);
std::string encodeChatProtocolMessage(const ChatProtocolMessage& message);
std::string encodeErrorResponse(ChatProtocolErrorCode code, const std::string& message);
JoinInfo parseInitialFrame(int clientFd, const std::string& initBody);