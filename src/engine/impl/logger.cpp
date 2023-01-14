#include "logger.h"

#include <chrono>
#include <sstream>
#include <unordered_map>

#if __ANDROID__
#error Impl here
#else
#include <iostream>
#endif

inline std::unordered_map<engine::log::LogLevel, const char*> LOG_LEVEL_LUT =
{
    { engine::log::LogLevel::eTrace,      "TRACE" },
    { engine::log::LogLevel::eCritical,   "CRITICAL" },
};

template<typename Stream, typename T>
inline Stream& streamin(Stream& s, const T& t)
{
    s << t;
    return s;
}

template<typename Stream, typename T>
inline Stream& streamin_with_braces(Stream& s, const T& t)
{
    s << "[" << t << "]";
    return s;
}

void engine::log::log(LogLevel level, std::string_view msg)
{
    const auto system_clock_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto local_time = std::localtime(&system_clock_now);
    
    std::stringstream stream;
    streamin_with_braces(stream, std::put_time(local_time, "%Y-%m-%d %X")) << " ";
    streamin_with_braces(stream, LOG_LEVEL_LUT[level]) << " ";
    streamin(stream, msg) << "\n";

#if __ANDROID__
#error Impl here
#else
    std::cout << stream.str();
#endif
}
