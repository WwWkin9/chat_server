#pragma once

#include "ChatRoom.h"

#include <cstddef>
#include <vector>
#include <unordered_map>
#include <mutex>

class ChatRoomManager
{
public:
    explicit ChatRoomManager(std::size_t shardCount = 0);
    ChatRoom::Ptr getOrCreateRoom(uint32_t roomId);
    std::vector<ChatRoom::Ptr> getAllRooms();

private:
    struct Shard
    {
        std::unordered_map<uint32_t, ChatRoom::Ptr> rooms;
        std::mutex mtx;
    };

    std::size_t shardCount_ = 0;
    std::vector<Shard> shards_;

    std::size_t shardIndex(uint32_t roomId) const;
};
