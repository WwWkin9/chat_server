#pragma once

#include <string>
#include <unordered_map>

struct Config
{
    int port = 8080;
    int backlog = 128;
    int clientIdleTimeoutSeconds = 60 * 15; // 15 minutes
    std::size_t maxOutputBufferBytes = 1024 * 1024; // 1MB per-client
    int roomLimit = 1000;
    std::string logLevel = "info";
    std::string tlsCertPath;
    std::string tlsKeyPath;
    std::unordered_map<std::string, std::string> authTokenToUser; // token -> username
    std::string dbPath; // path to sqlite DB file; if empty, persistence disabled

    // Rate limiting and backpressure
    std::size_t maxInputFrameBytes = 16 * 1024; // max size per incoming frame
    int connMsgPerSecond = 20; // per-connection messages per second
    int userMsgPerSecond = 50; // per-username messages per second
    int roomMsgPerSecond = 500; // per-room messages per second
    int globalMsgPerSecond = 5000; // global server-wide messages per second
    int maxBroadcastFanout = 1000; // max recipients per broadcast
    std::size_t outputHighWaterBytes = 512 * 1024; // per-client high watermark
    std::string rejectStrategy = "disconnect"; // disconnect: 断开慢客户端; drop: 直接丢弃; reject: 反馈给发送者并停止本次广播
    int gracefulShutdownSeconds = 10; // seconds to wait for in-flight requests
    bool persistOnShutdown = true; // persist rooms/members on shutdown
    int reactorThreads = 0; // 0 means auto-detect from hardware concurrency
    int roomShardCount = 0; // 0 means auto-detect from hardware concurrency

    // Load config from optional dotenv-style file and environment variables.
    // File format: KEY=VALUE per line. Environment variables override file values.
    static Config loadFromEnvOrFile(const std::string& pathEnvVar = "CHAT_SERVER_CONFIG");
};
