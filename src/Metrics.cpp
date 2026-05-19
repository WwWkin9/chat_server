#include "../include/Metrics.h"
#include <sstream>
#include <algorithm>

Metrics& Metrics::instance()
{
    static Metrics m;
    return m;
}

Metrics::Metrics() = default;

void Metrics::incAccepted() { accepted_.fetch_add(1, std::memory_order_relaxed); }
void Metrics::incDisconnected(const std::string& reason)
{
    disconnected_.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(mtx_);
    disconnectReasons_[reason]++;
}
void Metrics::incMessagesReceived() { messagesReceived_.fetch_add(1, std::memory_order_relaxed); }
void Metrics::incMessagesBroadcasted() { messagesBroadcasted_.fetch_add(1, std::memory_order_relaxed); }
void Metrics::incBroadcastFailures() { broadcastFailures_.fetch_add(1, std::memory_order_relaxed); }
void Metrics::updateQueueDepth(int fd, size_t bytes)
{
    totalQueueBytes_.store(static_cast<int>(bytes), std::memory_order_relaxed);
    int prevMax = maxQueueBytes_.load(std::memory_order_relaxed);
    if ((int)bytes > prevMax) maxQueueBytes_.store(static_cast<int>(bytes), std::memory_order_relaxed);
}
void Metrics::setRoomCount(int r) { roomCount_.store(r, std::memory_order_relaxed); }

std::string Metrics::toJson()
{
    std::ostringstream ss;
    ss << "{"
       << "\"accepted\":" << accepted_.load() << ","
       << "\"disconnected\":" << disconnected_.load() << ","
       << "\"messages_received\":" << messagesReceived_.load() << ","
       << "\"messages_broadcasted\":" << messagesBroadcasted_.load() << ","
       << "\"broadcast_failures\":" << broadcastFailures_.load() << ","
       << "\"queue_bytes\":" << totalQueueBytes_.load() << ","
       << "\"max_queue_bytes\":" << maxQueueBytes_.load() << ","
       << "\"room_count\":" << roomCount_.load();

    // include disconnect reasons
    ss << ",\"disconnect_reasons\":{";
    {
        std::lock_guard<std::mutex> lk(mtx_);
        bool first = true;
        for (auto &p : disconnectReasons_)
        {
            if (!first) ss << ",";
            first = false;
            ss << "\"" << p.first << "\":" << p.second;
        }
    }
    ss << "}}";
    return ss.str();
}
