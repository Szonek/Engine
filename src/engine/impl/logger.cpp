#include "logger.h"

#include <chrono>
#include <sstream>
#include <unordered_map>

#if __ANDROID__
#error Impl here
#else
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#endif

struct LogLevelTraits
{
    const char* name;
    fmt::color color;
};

inline std::unordered_map<engine::log::LogLevel, LogLevelTraits> LOG_LEVEL_LUT =
{
    { engine::log::LogLevel::eTrace,      {"TRACE", fmt::color::white} },
    { engine::log::LogLevel::eError,      {"ERROR", fmt::color::medium_violet_red} },
    { engine::log::LogLevel::eCritical,   {"CRITICAL", fmt::color::red} },
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
    const auto log_level_trait = LOG_LEVEL_LUT[level];

    //const auto system_clock_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    //const auto local_time = std::localtime(&system_clock_now);
    //
    //std::stringstream stream;
    //streamin_with_braces(stream, std::put_time(local_time, "%Y-%m-%d %X")) << " ";
    //streamin_with_braces(stream, LOG_LEVEL_LUT[level]) << " ";
    //streamin(stream, msg) << "\n";

    const auto printable_str = fmt::format("[{}][{}]: {}", std::chrono::system_clock::now(), log_level_trait.name, msg);

#if __ANDROID__
#error Impl here
#else
    fmt::print(fmt::fg(log_level_trait.color), printable_str);
#endif
}
