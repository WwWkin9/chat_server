#include "../include/ChatProtocol.h"

#include <cctype>
#include <limits>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
constexpr std::string_view kFramePrefix = "CHAT";
constexpr std::string_view kJoinType = "JOIN";
constexpr std::string_view kMessageType = "MSG";
constexpr std::string_view kAuthType = "AUTH";
constexpr std::string_view kRegisterType = "REGISTER";
constexpr std::string_view kLoginType = "LOGIN";
constexpr std::string_view kAuthAckType = "AUTH_ACK";
constexpr std::string_view kHeartbeatType = "PING";
constexpr std::string_view kErrorType = "ERR";

bool appendError(ChatProtocolErrorCode* errorCode, std::string* errorMessage, ChatProtocolErrorCode code,
                 const std::string& text)
{
    if (errorCode)
    {
        *errorCode = code;
    }
    if (errorMessage)
    {
        *errorMessage = text;
    }
    return false;
}

bool isAsciiDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

bool parseUnsignedInt(const std::string& text, int& value)
{
    if (text.empty())
    {
        return false;
    }

    long long result = 0;
    for (char ch : text)
    {
        if (!isAsciiDigit(ch))
        {
            return false;
        }

        result = result * 10 + (ch - '0');
        if (result > std::numeric_limits<int>::max())
        {
            return false;
        }
    }

    value = static_cast<int>(result);
    return true;
}

bool isValidUtf8(const std::string& text)
{
    std::size_t i = 0;
    while (i < text.size())
    {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 0;

        if ((lead & 0x80U) == 0U)
        {
            ++i;
            continue;
        }
        if ((lead & 0xE0U) == 0xC0U)
        {
            length = 2;
            if (lead < 0xC2U)
            {
                return false;
            }
        }
        else if ((lead & 0xF0U) == 0xE0U)
        {
            length = 3;
        }
        else if ((lead & 0xF8U) == 0xF0U)
        {
            length = 4;
            if (lead > 0xF4U)
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (i + length > text.size())
        {
            return false;
        }

        for (std::size_t offset = 1; offset < length; ++offset)
        {
            if ((static_cast<unsigned char>(text[i + offset]) & 0xC0U) != 0x80U)
            {
                return false;
            }
        }

        if (length == 3)
        {
            const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
            if (lead == 0xE0U && b1 < 0xA0U)
            {
                return false;
            }
            if (lead == 0xEDU && b1 >= 0xA0U)
            {
                return false;
            }
        }
        else if (length == 4)
        {
            const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
            if (lead == 0xF0U && b1 < 0x90U)
            {
                return false;
            }
            if (lead == 0xF4U && b1 >= 0x90U)
            {
                return false;
            }
        }

        i += length;
    }

    return true;
}

bool hasReservedBytes(const std::string& text)
{
    for (char ch : text)
    {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (value == 0U || value == '\r' || value == '\n' || value == '|')
        {
            return true;
        }
    }
    return false;
}

bool validateTextField(const std::string& text, std::size_t maxBytes, bool allowEmpty,
                       ChatProtocolErrorCode* errorCode, std::string* errorMessage)
{
    if (text.size() > maxBytes)
    {
        return appendError(errorCode, errorMessage, ChatProtocolErrorCode::FieldTooLong,
                           "field exceeds maximum length");
    }

    if (!allowEmpty && text.empty())
    {
        return appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidField,
                           "field must not be empty");
    }

    if (!isValidUtf8(text))
    {
        return appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidEncoding,
                           "field is not valid UTF-8");
    }

    if (hasReservedBytes(text))
    {
        return appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidField,
                           "field contains reserved characters");
    }

    return true;
}

bool validateUsername(const std::string& username, ChatProtocolErrorCode* errorCode, std::string* errorMessage)
{
    return validateTextField(username, kChatProtocolMaxUsernameBytes, false, errorCode, errorMessage);
}

bool validateMessageText(const std::string& text, ChatProtocolErrorCode* errorCode, std::string* errorMessage)
{
    return validateTextField(text, kChatProtocolMaxMessageBytes, false, errorCode, errorMessage);
}

bool validatePassword(const std::string& text, ChatProtocolErrorCode* errorCode, std::string* errorMessage)
{
    return validateTextField(text, kChatProtocolMaxPasswordBytes, false, errorCode, errorMessage);
}

bool validateErrorMessage(const std::string& text, ChatProtocolErrorCode* errorCode, std::string* errorMessage)
{
    return validateTextField(text, kChatProtocolMaxErrorMessageBytes, false, errorCode, errorMessage);
}

std::vector<std::string> split(const std::string& text, char delimiter)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t end = text.find(delimiter, start);
        if (end == std::string::npos)
        {
            parts.emplace_back(text.substr(start));
            break;
        }

        parts.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

ChatProtocolMessage makeJoinMessage(int roomId, std::string username)
{
    return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::Join,
                               ChatProtocolPayload{ChatJoinMessage{roomId, std::move(username)}}};
}

ChatProtocolMessage makeTextMessage(std::string text)
{
    return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::Message,
                               ChatProtocolPayload{ChatTextMessage{std::move(text)}}};
}

ChatProtocolMessage makeHeartbeatMessage()
{
    return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::Heartbeat,
                               ChatProtocolPayload{ChatHeartbeatMessage{}}};
}

ChatProtocolMessage makeErrorMessage(ChatProtocolErrorCode code, std::string message)
{
    return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::ErrorResponse,
                               ChatProtocolPayload{ChatErrorResponse{code, std::move(message)}}};
}

std::string encodeVersionedFrame(std::string_view type, const std::string& payload)
{
    std::string body;
    body.reserve(kFramePrefix.size() + type.size() + payload.size() + 8);
    body.append(kFramePrefix);
    body.push_back('|');
    body.append(std::to_string(kChatProtocolVersion));
    body.push_back('|');
    body.append(type);
    if (!payload.empty())
    {
        body.push_back('|');
        body.append(payload);
    }
    return body;
}

} // namespace

std::optional<ChatProtocolMessage> parseChatProtocolMessage(const std::string& frameBody,
                                                           ChatProtocolErrorCode* errorCode,
                                                           std::string* errorMessage)
{
    if (frameBody.size() > kChatProtocolMaxFrameBytes)
    {
        appendError(errorCode, errorMessage, ChatProtocolErrorCode::FieldTooLong,
                    "frame exceeds maximum length");
        return std::nullopt;
    }

    if (!isValidUtf8(frameBody))
    {
        appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidEncoding,
                    "frame body is not valid UTF-8");
        return std::nullopt;
    }

    if (frameBody.empty())
    {
        return makeHeartbeatMessage();
    }

    if (frameBody.rfind("CHAT|", 0) == 0)
    {
        const std::vector<std::string> parts = split(frameBody, '|');
        if (parts.size() < 3)
        {
            appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                        "versioned frame is missing required fields");
            return std::nullopt;
        }

        int version = 0;
        if (!parseUnsignedInt(parts[1], version))
        {
            appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidField,
                        "protocol version is invalid");
            return std::nullopt;
        }

        if (version != static_cast<int>(kChatProtocolVersion))
        {
            appendError(errorCode, errorMessage, ChatProtocolErrorCode::UnsupportedVersion,
                        "unsupported protocol version");
            return std::nullopt;
        }

        const std::string& type = parts[2];
        if (type == kJoinType)
        {
            if (parts.size() != 5)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "JOIN frame must contain room id and username");
                return std::nullopt;
            }

            int roomId = 0;
            if (!parseUnsignedInt(parts[3], roomId) || roomId <= 0)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidField,
                            "room id must be a positive integer");
                return std::nullopt;
            }

            if (!validateUsername(parts[4], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return makeJoinMessage(roomId, parts[4]);
        }

        if (type == kMessageType)
        {
            if (parts.size() != 4)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "message frame must contain one text payload");
                return std::nullopt;
            }

            if (!validateMessageText(parts[3], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return makeTextMessage(parts[3]);
        }

        if (type == kHeartbeatType)
        {
            if (parts.size() != 3)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "heartbeat frame must not contain extra fields");
                return std::nullopt;
            }

            return makeHeartbeatMessage();
        }

        if (type == kErrorType)
        {
            if (parts.size() != 5)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "error frame must contain error code and message");
                return std::nullopt;
            }

            int code = 0;
            if (!parseUnsignedInt(parts[3], code))
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidField,
                            "error code is invalid");
                return std::nullopt;
            }

            if (!validateErrorMessage(parts[4], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return makeErrorMessage(static_cast<ChatProtocolErrorCode>(code), parts[4]);
        }

        // handle auth types
        if (type == kAuthType)
        {
            if (parts.size() != 4)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "AUTH frame must contain a token payload");
                return std::nullopt;
            }

            if (!validateMessageText(parts[3], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::Auth,
                                       ChatProtocolPayload{ChatAuthMessage{parts[3]}}};
        }

        if (type == kRegisterType)
        {
            if (parts.size() != 5)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "REGISTER frame must contain username and password");
                return std::nullopt;
            }

            if (!validateUsername(parts[3], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            if (!validatePassword(parts[4], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::Register,
                                       ChatProtocolPayload{ChatCredentialMessage{parts[3], parts[4]}}};
        }

        if (type == kLoginType)
        {
            if (parts.size() != 5)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "LOGIN frame must contain username and password");
                return std::nullopt;
            }

            if (!validateUsername(parts[3], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            if (!validatePassword(parts[4], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::Login,
                                       ChatProtocolPayload{ChatCredentialMessage{parts[3], parts[4]}}};
        }

        if (type == kAuthAckType)
        {
            if (parts.size() != 4)
            {
                appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                            "AUTH_ACK frame must contain username");
                return std::nullopt;
            }

            if (!validateUsername(parts[3], errorCode, errorMessage))
            {
                return std::nullopt;
            }

            return ChatProtocolMessage{kChatProtocolVersion, ChatMessageType::AuthAck,
                                       ChatProtocolPayload{ChatAuthAckMessage{parts[3]}}};
        }

        appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                    "unknown protocol message type");
        return std::nullopt;
    }

    if (frameBody.size() >= 5 && frameBody.rfind("JOIN ", 0) == 0)
    {
        std::istringstream iss(frameBody);
        std::string cmd;
        int roomId = 0;
        std::string username;
        if (iss >> cmd && iss >> roomId && iss >> username)
        {
            if (roomId > 0 && validateUsername(username, errorCode, errorMessage))
            {
                return makeJoinMessage(roomId, username);
            }
        }

        appendError(errorCode, errorMessage, ChatProtocolErrorCode::InvalidFormat,
                    "legacy JOIN frame is invalid");
        return std::nullopt;
    }

    if (!validateMessageText(frameBody, errorCode, errorMessage))
    {
        return std::nullopt;
    }

    return makeTextMessage(frameBody);
}

std::string encodeChatProtocolMessage(const ChatProtocolMessage& message)
{
    switch (message.type)
    {
    case ChatMessageType::Join:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatJoinMessage>)
            {
                return encodeVersionedFrame("JOIN", std::to_string(payload.roomId) + "|" + payload.username);
            }
            return {};
        }, message.payload);
    case ChatMessageType::Message:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatTextMessage>)
            {
                return encodeVersionedFrame("MSG", payload.text);
            }
            return {};
        }, message.payload);
    case ChatMessageType::Heartbeat:
        return encodeVersionedFrame("PING", "");
    case ChatMessageType::Auth:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatAuthMessage>)
            {
                return encodeVersionedFrame("AUTH", payload.token);
            }
            return {};
        }, message.payload);
    case ChatMessageType::Register:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatCredentialMessage>)
            {
                return encodeVersionedFrame("REGISTER", payload.username + "|" + payload.password);
            }
            return {};
        }, message.payload);
    case ChatMessageType::Login:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatCredentialMessage>)
            {
                return encodeVersionedFrame("LOGIN", payload.username + "|" + payload.password);
            }
            return {};
        }, message.payload);
    case ChatMessageType::AuthAck:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatAuthAckMessage>)
            {
                return encodeVersionedFrame("AUTH_ACK", payload.username);
            }
            return {};
        }, message.payload);
    case ChatMessageType::ErrorResponse:
        return std::visit([](const auto& payload) -> std::string {
            using PayloadT = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<PayloadT, ChatErrorResponse>)
            {
                return encodeVersionedFrame("ERR", std::to_string(static_cast<std::uint16_t>(payload.code)) + "|" + payload.message);
            }
            return {};
        }, message.payload);
    }

    return {};
}

std::string encodeErrorResponse(ChatProtocolErrorCode code, const std::string& message)
{
    ChatProtocolMessage frame = makeErrorMessage(code, message);
    return encodeChatProtocolMessage(frame);
}

JoinInfo parseInitialFrame(int clientFd, const std::string& initBody)
{
    JoinInfo info;
    info.username = "user" + std::to_string(clientFd);

    std::string errorMessage;
    const std::optional<ChatProtocolMessage> frame = parseChatProtocolMessage(initBody, nullptr, &errorMessage);
    if (!frame)
    {
        return info;
    }

    if (frame->type == ChatMessageType::Join)
    {
        const auto& join = std::get<ChatJoinMessage>(frame->payload);
        info.roomId = join.roomId;
        info.username = join.username;
        return info;
    }

    if (frame->type == ChatMessageType::Message)
    {
        info.pendingMessage = std::get<ChatTextMessage>(frame->payload).text;
    }

    return info;
}