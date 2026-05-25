#include "../include/ChatProtocol.h"
#include "../include/ChatRoomService.h"
#include "../include/ChatRoomManager.h"
#include "../include/Config.h"

#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <tuple>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(1);
    }
}

void testProtocolRoundTrip()
{
    {
        const auto parsed = parseChatProtocolMessage("");
        require(parsed.has_value(), "empty frame should parse");
        require(parsed->type == ChatMessageType::Heartbeat, "empty frame should be heartbeat");
    }

    {
        const auto parsed = parseChatProtocolMessage("CHAT|1|JOIN|42|alice");
        require(parsed.has_value(), "join frame should parse");
        require(parsed->type == ChatMessageType::Join, "join type");
        const auto& join = std::get<ChatJoinMessage>(parsed->payload);
        require(join.roomId == 42, "join room id");
        require(join.username == "alice", "join username");
        require(encodeChatProtocolMessage(*parsed) == "CHAT|1|JOIN|42|alice", "join encode");
    }

    {
        const auto parsed = parseChatProtocolMessage("CHAT|1|MSG|hello world");
        require(parsed.has_value(), "msg frame should parse");
        require(parsed->type == ChatMessageType::Message, "msg type");
        const auto& msg = std::get<ChatTextMessage>(parsed->payload);
        require(msg.text == "hello world", "msg text");
        require(encodeChatProtocolMessage(*parsed) == "CHAT|1|MSG|hello world", "msg encode");
    }

    {
        const auto parsed = parseChatProtocolMessage("CHAT|1|AUTH|secret-token");
        require(parsed.has_value(), "auth frame should parse");
        require(parsed->type == ChatMessageType::Auth, "auth type");
        const auto& auth = std::get<ChatAuthMessage>(parsed->payload);
        require(auth.token == "secret-token", "auth token");
        require(encodeChatProtocolMessage(*parsed) == "CHAT|1|AUTH|secret-token", "auth encode");
    }

    {
        const auto parsed = parseChatProtocolMessage("CHAT|1|AUTH_ACK|alice");
        require(parsed.has_value(), "auth ack frame should parse");
        require(parsed->type == ChatMessageType::AuthAck, "auth ack type");
        const auto& ack = std::get<ChatAuthAckMessage>(parsed->payload);
        require(ack.username == "alice", "auth ack username");
        require(encodeChatProtocolMessage(*parsed) == "CHAT|1|AUTH_ACK|alice", "auth ack encode");
    }

    {
        const std::string encoded = encodeErrorResponse(ChatProtocolErrorCode::RateLimited, "too fast");
        const auto parsed = parseChatProtocolMessage(encoded);
        require(parsed.has_value(), "error frame should parse");
        require(parsed->type == ChatMessageType::ErrorResponse, "error type");
        const auto& error = std::get<ChatErrorResponse>(parsed->payload);
        require(error.code == ChatProtocolErrorCode::RateLimited, "error code");
        require(error.message == "too fast", "error message");
    }

    {
        const auto parsed = parseChatProtocolMessage("CHAT|2|PING", nullptr, nullptr);
        require(!parsed.has_value(), "unsupported version should fail");
        ChatProtocolErrorCode code = ChatProtocolErrorCode::InvalidFormat;
        std::string message;
        parseChatProtocolMessage("CHAT|2|PING", &code, &message);
        require(code == ChatProtocolErrorCode::UnsupportedVersion, "unsupported version code");
    }

    {
        std::string invalidUtf8 = "CHAT|1|MSG|";
        invalidUtf8.push_back(static_cast<char>(0xC3));
        invalidUtf8.push_back(static_cast<char>(0x28));
        ChatProtocolErrorCode code = ChatProtocolErrorCode::InvalidFormat;
        std::string message;
        const auto parsed = parseChatProtocolMessage(invalidUtf8, &code, &message);
        require(!parsed.has_value(), "invalid utf8 should fail");
        require(code == ChatProtocolErrorCode::InvalidEncoding, "invalid utf8 code");
    }

    {
        std::string longFrame(kChatProtocolMaxFrameBytes + 1, 'x');
        ChatProtocolErrorCode code = ChatProtocolErrorCode::InvalidFormat;
        std::string message;
        const auto parsed = parseChatProtocolMessage(longFrame, &code, &message);
        require(!parsed.has_value(), "oversized frame should fail");
        require(code == ChatProtocolErrorCode::FieldTooLong, "oversized frame code");
    }

    {
        const JoinInfo join = parseInitialFrame(7, "CHAT|1|JOIN|12|bob");
        require(join.roomId == 12, "initial join room");
        require(join.username == "bob", "initial join username");
        require(join.pendingMessage.empty(), "initial join pending message");
    }

    {
        const JoinInfo pending = parseInitialFrame(7, "hello room");
        require(pending.roomId == 0, "initial text room defaults to zero");
        require(pending.username == "user7", "initial text default username");
        require(pending.pendingMessage == "hello room", "initial text pending message");
    }
}

void testConfigLoading()
{
    char pathTemplate[] = "/tmp/chat_server_cfg_XXXXXX";
    const int fd = mkstemp(pathTemplate);
    require(fd >= 0, "mkstemp should succeed");
    close(fd);

    {
        std::ofstream out(pathTemplate, std::ios::trunc);
        out << "PORT=19091\n";
        out << "REACTOR_THREADS=3\n";
        out << "ROOM_SHARD_COUNT=5\n";
        out << "CLIENT_IDLE_TIMEOUT_SECONDS=7\n";
    }

    require(setenv("CHAT_SERVER_CONFIG", pathTemplate, 1) == 0, "setenv config path");
    require(setenv("CHAT_SERVER_PORT", "19092", 1) == 0, "setenv override port");
    require(setenv("CHAT_SERVER_REACTOR_THREADS", "4", 1) == 0, "setenv reactor threads");
    require(setenv("CHAT_SERVER_ROOM_SHARD_COUNT", "6", 1) == 0, "setenv shard count");

    const Config cfg = Config::loadFromEnvOrFile();
    require(cfg.port == 19092, "env should override file port");
    require(cfg.reactorThreads == 4, "reactor thread env");
    require(cfg.roomShardCount == 6, "room shard env");
    require(cfg.clientIdleTimeoutSeconds == 7, "file config should apply");

    unlink(pathTemplate);
}

void testRoomManager()
{
    ChatRoomManager manager(4);
    auto room1 = manager.getOrCreateRoom(11);
    auto room2 = manager.getOrCreateRoom(12);
    require(room1 && room2, "rooms should be created");
    require(manager.getAllRooms().size() >= 2, "manager should expose rooms");
}

void testBroadcastFailureStrategies()
{
    auto runScenario = [](const std::string& strategy) {
        ChatRoomManager manager(1);
        Config cfg;
        cfg.rejectStrategy = strategy;
        ChatRoomService service(manager, cfg);

        ChatSession alice;
        ChatSession bob;

        std::vector<std::pair<int, std::string>> sends;
        std::vector<std::pair<int, std::string>> disconnects;
        bool failBobSend = false;

        const auto sendMessage = [&](int fd, const std::string& msg) {
            sends.emplace_back(fd, msg);
            return !(failBobSend && fd == 2);
        };
        const auto disconnectClient = [&](int fd, const std::string& reason) {
            disconnects.emplace_back(fd, reason);
        };

        service.processIncomingFrame(1, alice, "CHAT|1|JOIN|1|alice", sendMessage, disconnectClient);
        service.processIncomingFrame(2, bob, "CHAT|1|JOIN|1|bob", sendMessage, disconnectClient);

        sends.clear();
        disconnects.clear();
        failBobSend = true;

        service.processIncomingFrame(1, alice, "CHAT|1|MSG|hello", sendMessage, disconnectClient);

        return std::make_tuple(sends, disconnects);
    };

    {
        const auto [sends, disconnects] = runScenario("drop");
        require(disconnects.empty(), "drop should not disconnect recipients");
        require(std::none_of(sends.begin(), sends.end(), [](const auto& item) { return item.first == 1 && item.second.find("broadcast rejected") != std::string::npos; }),
                "drop should not reject sender");
    }

    {
        const auto [sends, disconnects] = runScenario("disconnect");
        require(disconnects.size() == 1, "disconnect should close slow recipient");
        require(disconnects.front().first == 2, "disconnect should target recipient");
        require(disconnects.front().second == "output buffer full", "disconnect reason");
        require(std::none_of(sends.begin(), sends.end(), [](const auto& item) { return item.first == 1 && item.second.find("broadcast rejected") != std::string::npos; }),
                "disconnect should not reject sender");
    }

    {
        const auto [sends, disconnects] = runScenario("reject");
        require(disconnects.empty(), "reject should not disconnect recipient");
        require(std::any_of(sends.begin(), sends.end(), [](const auto& item) { return item.first == 1 && item.second.find("broadcast rejected due to slow recipient") != std::string::npos; }),
                "reject should notify sender");
    }
}
}

int main()
{
    testProtocolRoundTrip();
    testConfigLoading();
    testRoomManager();
    testBroadcastFailureStrategies();
    std::cout << "chat_server_tests passed" << std::endl;
    return 0;
}