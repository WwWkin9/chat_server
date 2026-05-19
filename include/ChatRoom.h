// ChatRoom.hpp
#pragma once

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <unordered_map>
#include <mutex>
#include "User.h"
#include <memory>

class ChatRoom
{
public:
    using Ptr = std::shared_ptr<ChatRoom>;
    explicit ChatRoom(uint32_t roomId) : roomId_(roomId) {}
    ~ChatRoom() = default;
    void addClient(const User::Ptr& user);
    void removeClient(const User::Ptr& user);
    
    std::vector<User::Ptr> snapshotClients();
    // Broadcast will iterate members and invoke the provided deliver callback
    // for each recipient (except the sender). If deliver is null, no action is taken.
    void broadcast(const std::string& msg, const User::Ptr& sender,
                   const std::function<void(const User::Ptr&, const std::string&)>& deliver = nullptr);

    std::string getName(const User::Ptr& user);
    User::Ptr getUser(const User::Ptr& user);
    uint32_t roomId() const { return roomId_; }
private:
    uint32_t roomId_;
    std::vector<User::Ptr> users_;
    std::unordered_map<uint32_t, User::Ptr> users_map_;
    std::mutex mtx_;
};
