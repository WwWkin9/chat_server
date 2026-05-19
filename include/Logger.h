#pragma once

#include <string>

namespace Logger
{
    void info(const std::string& msg, const std::string& jsonFields = "");
    void warn(const std::string& msg, const std::string& jsonFields = "");
    void error(const std::string& msg, const std::string& jsonFields = "");
}
