#pragma once

#include <string>
#include <vector>

struct OfflineMessage
{
    int id;
    int roomId;
    std::string recipient;
    std::string sender;
    std::string message;
    long long ts;
};

class Persistence
{
public:
    virtual ~Persistence() = default;
    virtual bool init(const std::string& dbPath) = 0;
    virtual bool addMembership(int roomId, const std::string& username) = 0;
    virtual std::vector<std::string> getRoomMembers(int roomId) = 0;
    virtual bool appendAudit(int roomId, const std::string& sender, const std::string& message, long long ts) = 0;
    virtual bool storeOfflineMessage(int roomId, const std::string& recipient, const std::string& sender, const std::string& message, long long ts) = 0;
    virtual std::vector<OfflineMessage> getOfflineMessages(const std::string& username) = 0;
    virtual bool clearOfflineMessagesFor(const std::string& username) = 0;
};
