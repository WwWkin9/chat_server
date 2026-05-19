#pragma once

#include <string>

bool queueFrame(std::string& outputBuffer, const std::string& payload);
bool flushOutput(int fd, std::string& outputBuffer);
bool receiveIntoBuffer(int fd, std::string& inputBuffer);
bool popFrame(std::string& inputBuffer, std::string& body);