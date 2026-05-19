#include "../include/ChatProtocol.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

int main(int argc, char** argv)
{
    std::mt19937_64 rng(123456);
    std::uniform_int_distribution<int> lenDist(0, static_cast<int>(kChatProtocolMaxFrameBytes));

    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i)
    {
        int len = lenDist(rng);
        std::string s;
        s.reserve(len);
        for (int j = 0; j < len; ++j)
        {
            char c = static_cast<char>(rng() & 0xFF);
            s.push_back(c);
        }
        ChatProtocolErrorCode code;
        std::string msg;
        // Ensure no UB or exceptions
        auto res = parseChatProtocolMessage(s, &code, &msg);
        (void)res;
    }

    std::cout << "fuzz run complete" << std::endl;
    return 0;
}
