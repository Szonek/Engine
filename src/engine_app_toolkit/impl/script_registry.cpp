#include "script_registry.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>

bool engine::ScriptRegistry::add_type(std::string_view name, std::type_index cpp_type, Factory factory, std::vector<engine_component_type_t> required)
{
    // the editor's dropdown and scene files need unique names
    const auto used = std::ranges::any_of(types_, [&](const ScriptType& t) { return t.name == name || t.cpp_type == cpp_type; });
    if (used || name.empty() || name == "none" || component_type_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        log(fmt::format("Script type \"{}\" can't be registered: its name is empty, \"none\" or used, its type is registered already, or "
            "the script component exists already.\n", name));
        return false;
    }
    types_.push_back(ScriptType{ std::string(name), cpp_type, std::move(factory), std::move(required) });
    return true;
}

bool engine::ScriptRegistry::register_component()
{
    std::vector<engine_enum_value_desc_t> values;
    values.push_back({ "none", NO_SCRIPT });
    for (std::size_t i = 0; i < types_.size(); i++)
    {
        // the editor adds them in the same undo step as the script (like Unity's RequireComponent)
        const auto& required = types_[i].required;
        values.push_back({ types_[i].name.c_str(), static_cast<std::int32_t>(i + 1), required.empty() ? nullptr : required.data(),
            static_cast<std::uint32_t>(required.size()) });
    }
    engine_field_desc_t type_field{};
    type_field.name = "type";
    type_field.kind = ENGINE_FIELD_KIND_ENUM;
    type_field.offset = offsetof(ScriptComponent, type);
    type_field.enum_values = values.data();
    type_field.enum_values_count = static_cast<std::uint32_t>(values.size());
    type_field.tooltip = "the script this game object runs";

    const ScriptComponent default_value{ NO_SCRIPT };
    engine_type_desc_t desc{};
    desc.name = COMPONENT_NAME;
    desc.size = sizeof(ScriptComponent);
    desc.alignment = alignof(ScriptComponent);
    desc.default_value = &default_value;
    desc.fields = &type_field;
    desc.fields_count = 1;
    return engineComponentTypeRegister(&desc, &component_type_) == ENGINE_RESULT_CODE_OK;
}

const engine::ScriptRegistry::ScriptType* engine::ScriptRegistry::find(std::int32_t index) const
{
    return index > NO_SCRIPT && static_cast<std::size_t>(index) <= types_.size() ? &types_[index - 1] : nullptr;
}

std::int32_t engine::ScriptRegistry::find_index(std::type_index cpp_type) const
{
    const auto it = std::ranges::find(types_, cpp_type, &ScriptType::cpp_type);
    return it == types_.end() ? NO_SCRIPT : static_cast<std::int32_t>(it - types_.begin()) + 1;
}
