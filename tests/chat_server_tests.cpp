#include "../include/ChatProtocol.h"
#include "../include/ChatRoomManager.h"
#include "../include/Config.h"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
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
}

int main()
{
    testProtocolRoundTrip();
    testConfigLoading();
    testRoomManager();
    std::cout << "chat_server_tests passed" << std::endl;
    return 0;
}