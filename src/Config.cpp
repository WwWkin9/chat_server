#include "../include/Config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
static inline std::string getenv_or_empty(const char* name)
{
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

static inline void trim(std::string& s)
{
    size_t a = 0;
    while (a < s.size() && isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && isspace(static_cast<unsigned char>(s[b-1]))) --b;
    s = s.substr(a, b - a);
}
}

Config Config::loadFromEnvOrFile(const std::string& pathEnvVar)
{
    Config cfg;

    // If env var points to a file, parse it
    std::string filePath = getenv_or_empty(pathEnvVar.c_str());
    if (!filePath.empty())
    {
        std::ifstream in(filePath);
        if (in)
        {
            std::string line;
            while (std::getline(in, line))
            {
                trim(line);
                if (line.empty() || line[0] == '#') continue;
                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                trim(key);
                trim(val);
                if (key == "PORT") cfg.port = std::stoi(val);
                else if (key == "BACKLOG") cfg.backlog = std::stoi(val);
                else if (key == "CLIENT_IDLE_TIMEOUT_SECONDS") cfg.clientIdleTimeoutSeconds = std::stoi(val);
                else if (key == "MAX_OUTPUT_BUFFER_BYTES") cfg.maxOutputBufferBytes = static_cast<std::size_t>(std::stoull(val));
                else if (key == "ROOM_LIMIT") cfg.roomLimit = std::stoi(val);
                else if (key == "LOG_LEVEL") cfg.logLevel = val;
                else if (key == "TLS_CERT_PATH") cfg.tlsCertPath = val;
                else if (key == "TLS_KEY_PATH") cfg.tlsKeyPath = val;
                else if (key == "DB_PATH") cfg.dbPath = val;
                else if (key == "MAX_INPUT_FRAME_BYTES") cfg.maxInputFrameBytes = static_cast<std::size_t>(std::stoull(val));
                else if (key == "CONN_MSG_PER_SECOND") cfg.connMsgPerSecond = std::stoi(val);
                else if (key == "USER_MSG_PER_SECOND") cfg.userMsgPerSecond = std::stoi(val);
                else if (key == "ROOM_MSG_PER_SECOND") cfg.roomMsgPerSecond = std::stoi(val);
                else if (key == "GLOBAL_MSG_PER_SECOND") cfg.globalMsgPerSecond = std::stoi(val);
                else if (key == "MAX_BROADCAST_FANOUT") cfg.maxBroadcastFanout = std::stoi(val);
                else if (key == "OUTPUT_HIGH_WATER_BYTES") cfg.outputHighWaterBytes = static_cast<std::size_t>(std::stoull(val));
                else if (key == "REJECT_STRATEGY") cfg.rejectStrategy = val;
                else if (key == "GRACEFUL_SHUTDOWN_SECONDS") cfg.gracefulShutdownSeconds = std::stoi(val);
                else if (key == "PERSIST_ON_SHUTDOWN") cfg.persistOnShutdown = (val == "1" || val == "true");
                else if (key == "REACTOR_THREADS") cfg.reactorThreads = std::stoi(val);
                else if (key == "ROOM_SHARD_COUNT") cfg.roomShardCount = std::stoi(val);
                else if (key == "AUTH_TOKENS")
                {
                    // expected format: username:token,username2:token2
                    std::istringstream ss(val);
                    std::string item;
                    while (std::getline(ss, item, ','))
                    {
                        trim(item);
                        if (item.empty()) continue;
                        size_t colon = item.find(':');
                        if (colon == std::string::npos) continue;
                        std::string uname = item.substr(0, colon);
                        std::string tok = item.substr(colon + 1);
                        trim(uname);
                        trim(tok);
                        if (!uname.empty() && !tok.empty()) cfg.authTokenToUser.emplace(tok, uname);
                    }
                }
            }
        }
    }

    // Environment variables override file
    std::string v;
    v = getenv_or_empty("CHAT_SERVER_PORT"); if (!v.empty()) cfg.port = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_BACKLOG"); if (!v.empty()) cfg.backlog = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_CLIENT_IDLE_TIMEOUT_SECONDS"); if (!v.empty()) cfg.clientIdleTimeoutSeconds = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_MAX_OUTPUT_BUFFER_BYTES"); if (!v.empty()) cfg.maxOutputBufferBytes = static_cast<std::size_t>(std::stoull(v));
    v = getenv_or_empty("CHAT_SERVER_ROOM_LIMIT"); if (!v.empty()) cfg.roomLimit = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_LOG_LEVEL"); if (!v.empty()) cfg.logLevel = v;
    v = getenv_or_empty("CHAT_SERVER_TLS_CERT_PATH"); if (!v.empty()) cfg.tlsCertPath = v;
    v = getenv_or_empty("CHAT_SERVER_TLS_KEY_PATH"); if (!v.empty()) cfg.tlsKeyPath = v;
    v = getenv_or_empty("CHAT_SERVER_DB_PATH"); if (!v.empty()) cfg.dbPath = v;
    v = getenv_or_empty("CHAT_SERVER_MAX_INPUT_FRAME_BYTES"); if (!v.empty()) cfg.maxInputFrameBytes = static_cast<std::size_t>(std::stoull(v));
    v = getenv_or_empty("CHAT_SERVER_CONN_MSG_PER_SECOND"); if (!v.empty()) cfg.connMsgPerSecond = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_USER_MSG_PER_SECOND"); if (!v.empty()) cfg.userMsgPerSecond = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_ROOM_MSG_PER_SECOND"); if (!v.empty()) cfg.roomMsgPerSecond = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_GLOBAL_MSG_PER_SECOND"); if (!v.empty()) cfg.globalMsgPerSecond = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_MAX_BROADCAST_FANOUT"); if (!v.empty()) cfg.maxBroadcastFanout = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_OUTPUT_HIGH_WATER_BYTES"); if (!v.empty()) cfg.outputHighWaterBytes = static_cast<std::size_t>(std::stoull(v));
    v = getenv_or_empty("CHAT_SERVER_REJECT_STRATEGY"); if (!v.empty()) cfg.rejectStrategy = v;
    v = getenv_or_empty("CHAT_SERVER_GRACEFUL_SHUTDOWN_SECONDS"); if (!v.empty()) cfg.gracefulShutdownSeconds = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_PERSIST_ON_SHUTDOWN"); if (!v.empty()) cfg.persistOnShutdown = (v == "1" || v == "true");
    v = getenv_or_empty("CHAT_SERVER_REACTOR_THREADS"); if (!v.empty()) cfg.reactorThreads = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_ROOM_SHARD_COUNT"); if (!v.empty()) cfg.roomShardCount = std::stoi(v);
    v = getenv_or_empty("CHAT_SERVER_AUTH_TOKENS"); if (!v.empty())
    {
        std::istringstream ss(v);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            trim(item);
            if (item.empty()) continue;
            size_t colon = item.find(':');
            if (colon == std::string::npos) continue;
            std::string uname = item.substr(0, colon);
            std::string tok = item.substr(colon + 1);
            trim(uname);
            trim(tok);
            if (!uname.empty() && !tok.empty()) cfg.authTokenToUser.emplace(tok, uname);
        }
    }

    return cfg;
}
