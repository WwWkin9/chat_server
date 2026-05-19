#pragma once

#include <cstdint>
#include <memory>
#include <string>

// forward declare to avoid circular include
class ChatRoom;

class User
{
public:
    using Ptr = std::shared_ptr<User>;
    User(uint32_t id, const std::string& username, ChatRoom* room)
        : id_(id), username_(username), room_(room) {}
    ~User() = default;

    std::string username() const { return username_; }
    uint32_t id() const { return id_; }
    ChatRoom* room() const { return room_; }
private:
    uint32_t id_;
    std::string username_;
    ChatRoom* room_;
};