#include "../include/Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace Logger
{
    static std::string timestamp()
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    static void logLine(const std::string& level, const std::string& msg, const std::string& jsonFields)
    {
        if (jsonFields.empty())
            std::cout << "{\"ts\":\"" << timestamp() << "\",\"level\":\"" << level << "\",\"msg\":\"" << msg << "\"}" << std::endl;
        else
            std::cout << "{\"ts\":\"" << timestamp() << "\",\"level\":\"" << level << "\",\"msg\":\"" << msg << "\"," << jsonFields << "}" << std::endl;
    }

    void info(const std::string& msg, const std::string& jsonFields) { logLine("info", msg, jsonFields); }
    void warn(const std::string& msg, const std::string& jsonFields) { logLine("warn", msg, jsonFields); }
    void error(const std::string& msg, const std::string& jsonFields) { logLine("error", msg, jsonFields); }
}
