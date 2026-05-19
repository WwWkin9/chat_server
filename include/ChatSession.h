#pragma once

#include "ChatRoom.h"

#include <string>

struct ChatSession
{
    bool joined = false;
    bool authenticated = false;
    std::string authToken;
    int roomId = 0;
    std::string username;
    ChatRoom::Ptr room;
    User::Ptr user;
};