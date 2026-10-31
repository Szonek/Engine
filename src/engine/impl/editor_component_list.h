#pragma once
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
// What the properties panel's "Add Component" list offers (like Unity's Add Component button and Unreal's + Add): the
// components a game object doesn't have yet, by category, narrowed down by what is typed. GL-free.
struct component_choice_t
{
    std::string name;
    std::string category;
};

struct component_group_t
{
    std::string category;
    // indices into the choices, in their order
    std::vector<std::size_t> choices;
};

// whether `text` contains `part`, ignoring case (ASCII); an empty part is in every text
inline bool contains_ignoring_case(std::string_view text, std::string_view part)
{
    if (part.size() > text.size())
    {
        return false;
    }
    const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    for (std::size_t begin = 0; begin + part.size() <= text.size(); begin++)
    {
        std::size_t i = 0;
        while (i < part.size() && lower(text[begin + i]) == lower(part[i]))
        {
            i++;
        }
        if (i == part.size())
        {
            return true;
        }
    }
    return false;
}

// The choices whose name or category contains `filter` (ignoring case), grouped by category in the order the
// categories first appear in the choices; groups without a match are left out.
inline std::vector<component_group_t> group_component_choices(const std::vector<component_choice_t>& choices, std::string_view filter)
{
    std::vector<component_group_t> groups;
    for (std::size_t i = 0; i < choices.size(); i++)
    {
        const auto& choice = choices[i];
        if (!contains_ignoring_case(choice.name, filter) && !contains_ignoring_case(choice.category, filter))
        {
            continue;
        }
        auto it = groups.begin();
        while (it != groups.end() && it->category != choice.category)
        {
            ++it;
        }
        if (it == groups.end())
        {
            groups.push_back(component_group_t{ choice.category, {} });
            it = groups.end() - 1;
        }
        it->choices.push_back(i);
    }
    return groups;
}

// the choices of the groups in the order they are listed (what the arrow keys move through)
inline std::vector<std::size_t> listed_component_choices(const std::vector<component_group_t>& groups)
{
    std::vector<std::size_t> listed;
    for (const auto& group : groups)
    {
        listed.insert(listed.end(), group.choices.begin(), group.choices.end());
    }
    return listed;
}
}  // namespace engine
