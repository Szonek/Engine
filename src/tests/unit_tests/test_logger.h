#pragma once
#include <string>
#include <string_view>
#include <vector>

// The tests provide the logger the engine library would; it records what was logged.
namespace test_logger
{
std::vector<std::string>& messages();
// whether a logged message contains the text
bool logged(std::string_view text);
}  // namespace test_logger
