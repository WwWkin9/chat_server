// ChatRoom.cpp

#include "../include/ChatRoom.h"
#include "../include/Utils.h"

#include <algorithm>

void ChatRoom::addClient(const User::Ptr& user)
{
    if (!user)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_);
    users_.push_back(user);
    users_map_[user->id()] = user;
}

void ChatRoom::removeClient(const User::Ptr& user)
{
    if (!user)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_);
    users_map_.erase(user->id());
    users_.erase(std::remove(users_.begin(), users_.end(), user), users_.end());
}

std::vector<User::Ptr> ChatRoom::snapshotClients()
{
    std::lock_guard<std::mutex> lock(mtx_);
    return users_;
}

void ChatRoom::broadcast(const std::string& msg, const User::Ptr& sender,
                         const std::function<void(const User::Ptr&, const std::string&)>& deliver)
{
    if (!deliver)
    {
        // 未提供投递回调时，ChatRoom 不会直接执行 I/O。
        return;
    }

    const uint32_t senderId = sender ? sender->id() : static_cast<uint32_t>(-1);
    auto clients = snapshotClients(); // 避免持锁调用回调导致性能问题
    for (const auto& user : clients)
    {
        if (!user)
        {
            continue;
        }

        if (user->id() == senderId)
        {
            continue;
        }

        deliver(user, msg);
    }
}

std::string ChatRoom::getName(const User::Ptr& user)
{
    if (!user)
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = users_map_.find(user->id());
    return it == users_map_.end() ? std::string() : it->second->username();
}

User::Ptr ChatRoom::getUser(const User::Ptr& user)
{
    if (!user)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = users_map_.find(user->id());
    return it == users_map_.end() ? nullptr : it->second;
}