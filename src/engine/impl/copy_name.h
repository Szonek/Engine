#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
// The name of a copy (the editor's duplicate): "<base> (<n>)" with the first n from 1 which no name in `names` uses.
// The base is the name without a number in parentheses at its end, so a copy of "wall (1)" is "wall (2)". The result
// is shorter than `max_length` (the size of the name component's storage, the terminator included): the number is
// kept and the base is cut, never inside a UTF-8 character.
inline std::string copy_name(std::string_view name, const std::vector<std::string>& names, std::size_t max_length)
{
    auto base = std::string(name);
    if (const auto open = base.rfind(" ("); open != std::string::npos && base.ends_with(')')
        && open + 2 < base.size() - 1 && std::all_of(base.begin() + open + 2, base.end() - 1, [](char c) { return c >= '0' && c <= '9'; }))
    {
        base.resize(open);
    }
    for (std::size_t n = 1;; n++)
    {
        const auto suffix = " (" + std::to_string(n) + ")";
        auto length = std::min(base.size(), max_length - 1 - suffix.size());
        // a UTF-8 continuation byte (10xxxxxx) can't start the cut part
        while (length > 0 && length < base.size() && (static_cast<unsigned char>(base[length]) & 0xC0) == 0x80)
        {
            length--;
        }
        auto candidate = base.substr(0, length) + suffix;
        if (std::find(names.begin(), names.end(), candidate) == names.end())
        {
            return candidate;
        }
    }
}
}  // namespace engine
