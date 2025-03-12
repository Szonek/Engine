#include "logger.h"

#include <chrono>
#include <unordered_map>
#include <regex>

#ifdef _MSC_VER
#pragma warning(disable: 4127) // disable warning
#endif
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/color.h>
#ifdef _MSC_VER
#pragma warning(default: 4127) // enable warning back
#endif

#if __ANDROID__
#include <android/log.h>
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

inline std::string escape_curly_braces(std::string result)
{
    result = std::regex_replace(result, std::regex("\\{"), "{{");
    result = std::regex_replace(result, std::regex("\\}"), "}}");
    return result;
}


void engine::log::log(LogLevel level, std::string_view msg)
{
    const auto log_level_trait = LOG_LEVEL_LUT[level];
    const auto escaped_msg = std::string(msg);// escape_curly_braces(std::string(msg));  // std::format do not like {} in msg, it has to be doubled

    const auto printable_str = fmt::format("[{}][{}]: {}", std::chrono::system_clock::now(), log_level_trait.name, escaped_msg);
#if __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "", "%s", printable_str.data());
#else
    fmt::print(fmt::fg(log_level_trait.color), printable_str);
#endif
}
