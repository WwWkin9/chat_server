// Utils.hpp - 网络帧收发辅助
#pragma once

#include <string>

bool recv_all(int fd, char* buffer, size_t size);
bool send_full(int fd, const char* buffer, size_t size);
bool send_frame(int fd, const std::string& payload);
bool read_frame(int fd, std::string& body);
