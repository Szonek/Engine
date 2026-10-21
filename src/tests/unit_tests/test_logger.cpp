#include "test_logger.h"
#include "logger.h"

#include <algorithm>

std::vector<std::string>& test_logger::messages()
{
    static std::vector<std::string> messages;
    return messages;
}

bool test_logger::logged(std::string_view text)
{
    return std::ranges::any_of(messages(), [text](const std::string& m) { return m.find(text) != std::string::npos; });
}

void engine::log::log(engine::log::LogLevel /*level*/, std::string_view msg)
{
    test_logger::messages().emplace_back(msg);
}
