#include "../include/SocketFrameIO.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

bool queueFrame(std::string& outputBuffer, const std::string& payload)
{
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint32_t netlen = htonl(len);
    outputBuffer.append(reinterpret_cast<const char*>(&netlen), sizeof(netlen));
    outputBuffer.append(payload);
    return true;
}

bool flushOutput(int fd, std::string& outputBuffer)
{
    while (!outputBuffer.empty())
    {
        ssize_t sent = send(fd, outputBuffer.data(), outputBuffer.size(), 0);
        if (sent > 0)
        {
            outputBuffer.erase(0, static_cast<size_t>(sent));
            continue;
        }

        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return true;
        }

        return false;
    }

    return true;
}

bool receiveIntoBuffer(int fd, std::string& inputBuffer)
{
    char buffer[4096];
    while (true)
    {
        ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
        if (received > 0 )
        {
            inputBuffer.append(buffer, static_cast<size_t>(received));
            continue; // 继续读取直到没有数据可读
        }

        if (received == 0)
        {
            return false; // 客户端关闭连接
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return true; // 没有更多数据可读
        }

        return false; // 发生错误
    }
}

bool popFrame(std::string& inputBuffer, std::string& body)
{
    if (inputBuffer.size() < sizeof(uint32_t))
    {
        return false;
    }

    uint32_t netlen = 0;
    std::memcpy(&netlen, inputBuffer.data(), sizeof(netlen));
    uint32_t len = ntohl(netlen);
    if (inputBuffer.size() < sizeof(uint32_t) + len)
    {
        return false;
    }

    body.assign(inputBuffer.data() + sizeof(uint32_t), len);
    inputBuffer.erase(0, sizeof(uint32_t) + len);
    return true;
}