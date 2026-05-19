#include "ChatRoomManager.h"

#include <algorithm>
#include <memory>
#include <thread>

namespace
{
std::size_t detectShardCount(std::size_t requested)
{
    if (requested > 0)
    {
        return requested;
    }

    const unsigned int hw = std::thread::hardware_concurrency();
    return hw > 0 ? static_cast<std::size_t>(hw) : 4;
}
}

ChatRoomManager::ChatRoomManager(std::size_t shardCount)
    : shardCount_(detectShardCount(shardCount)), shards_(shardCount_)
{
}

std::size_t ChatRoomManager::shardIndex(uint32_t roomId) const
{
    return shardCount_ == 0 ? 0 : static_cast<std::size_t>(roomId) % shardCount_;
}

ChatRoom::Ptr ChatRoomManager::getOrCreateRoom(uint32_t roomId)
{
    auto& shard = shards_[shardIndex(roomId)];
    std::lock_guard<std::mutex> lock(shard.mtx);
    auto it = shard.rooms.find(roomId);
    if (it != shard.rooms.end())
    {
        return it->second;
    }
    else
    {
        auto room = std::make_shared<ChatRoom>(roomId);
        shard.rooms[roomId] = room;
        return room;
    }
}

std::vector<ChatRoom::Ptr> ChatRoomManager::getAllRooms()
{
    std::vector<ChatRoom::Ptr> res;
    for (auto& shard : shards_)
    {
        std::lock_guard<std::mutex> lock(shard.mtx);
        res.reserve(res.size() + shard.rooms.size());
        for (auto& p : shard.rooms)
        {
            res.push_back(p.second);
        }
    }
    return res;
}
