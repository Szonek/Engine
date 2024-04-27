#pragma once
#include <string_view>

namespace engine
{
namespace log
{
enum class LogLevel
{
    eTrace = 0,
    eError = 1,
    eCritical
};

void log(LogLevel level, std::string_view msg);

} // namespace log
} // namespace engine