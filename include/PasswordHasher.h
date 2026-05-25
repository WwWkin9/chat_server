#pragma once

#include <cstddef>
#include <string>

std::string generateSaltHex(std::size_t numBytes = 16);
std::string hashPassword(const std::string& password, const std::string& saltHex);
bool verifyPassword(const std::string& password, const std::string& saltHex, const std::string& expectedHashHex);
