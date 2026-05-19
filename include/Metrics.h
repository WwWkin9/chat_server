#pragma once

#include <string>
#include <atomic>
#include <unordered_map>
#include <mutex>

class Metrics
{
public:
    static Metrics& instance();

    void incAccepted();
    void incDisconnected(const std::string& reason);
    void incMessagesReceived();
    void incMessagesBroadcasted();
    void incBroadcastFailures();
    void updateQueueDepth(int fd, size_t bytes);
    void setRoomCount(int r);

    std::string toJson();

private:
    Metrics();
    std::atomic<int> accepted_{0};
    std::atomic<int> disconnected_{0};
    std::atomic<int> messagesReceived_{0};
    std::atomic<int> messagesBroadcasted_{0};
    std::atomic<int> broadcastFailures_{0};
    std::atomic<int> totalQueueBytes_{0};
    std::atomic<int> maxQueueBytes_{0};
    std::atomic<int> roomCount_{0};
    std::mutex mtx_;
    std::unordered_map<std::string,int> disconnectReasons_;
};
