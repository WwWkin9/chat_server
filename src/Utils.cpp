// Utils.cpp

#include "../include/Utils.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>

bool recv_all(int fd, char* buffer, size_t size)
{
    size_t total = 0;
    while (total < size) {
        ssize_t n = recv(fd, buffer + total, size - total, 0);
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

bool send_full(int fd, const char* buffer, size_t size)
{
    size_t totalSent = 0;
    while (totalSent < size)
    {
        ssize_t bytesSent = send(fd, buffer + totalSent, size - totalSent, 0);
        if (bytesSent <= 0)
        {
            break;
        }
        totalSent += bytesSent;
    }
    return totalSent == size;
}

bool send_frame(int fd, const std::string& payload)
{
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint32_t netlen = htonl(len);
    if (!send_full(fd, reinterpret_cast<const char*>(&netlen), sizeof(netlen))) return false;
    if (len == 0) return true;
    return send_full(fd, payload.data(), payload.size());
}

bool read_frame(int fd, std::string& body)
{
    uint32_t netlen = 0;
    if (!recv_all(fd, reinterpret_cast<char*>(&netlen), sizeof(netlen)))
    {
        return false;
    }

    uint32_t len = ntohl(netlen);
    body.clear();
    if (len > 0)
    {
        body.resize(len);
        if (!recv_all(fd, body.data(), len))
        {
            return false;
        }
    }

    return true;
}
