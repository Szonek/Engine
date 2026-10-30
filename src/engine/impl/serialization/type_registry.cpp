#include "type_registry.h"
#include "../logger.h"

#include <cstring>
#include <set>
#include <string_view>

void engine::type_builder_detail::report_error(std::string_view type_name, std::string_view message)
{
    log::log(log::LogLevel::eError, fmt::format("Type {}: {}. It is not registered.\n", type_name, message));
}

const engine::TypeInfo* engine::TypeRegistry::add(TypeInfo&& type)
{
    if (type.name.empty() || find(type.name))
    {
        log::log(log::LogLevel::eError, fmt::format("Type registry: a type needs a unique name, \"{}\" is empty or registered already.\n", type.name));
        return nullptr;
    }
    type.handle = static_cast<std::uint32_t>(types_.size()) + FIRST_TYPE_HANDLE;
    types_.push_back(std::make_unique<TypeInfo>(std::move(type)));
    if (types_.back()->is_component())
    {
        components_.push_back(types_.back().get());
    }
    return types_.back().get();
}

const engine::TypeInfo* engine::TypeRegistry::find(std::string_view name) const
{
    const auto it = std::ranges::find_if(types_, [name](const auto& t) { return t->name == name; });
    return it == types_.end() ? nullptr : it->get();
}

const engine::TypeInfo* engine::TypeRegistry::get(std::uint32_t handle) const
{
    return handle >= FIRST_TYPE_HANDLE && handle - FIRST_TYPE_HANDLE < types_.size() ? types_[handle - FIRST_TYPE_HANDLE].get() : nullptr;
}

std::vector<const engine::TypeInfo*> engine::TypeRegistry::get_types() const
{
    std::vector<const TypeInfo*> ret;
    for (const auto& t : types_)
    {
        ret.push_back(t.get());
    }
    return ret;
}

std::string engine::type_builder_detail::add_union_case(const TypeRegistry& registry, const FieldInfo& tag, std::int32_t value, std::string_view type_name, std::size_t max_size, FieldInfo& union_field)
{
    const auto* case_type = registry.find(type_name);
    // the case is written under the name of its tag value: "type": "box", "box": { ... }
    const auto tag_value = std::ranges::find(tag.enum_values, value, &EnumValue::value);
    if (!case_type || case_type->size > max_size || tag_value == tag.enum_values.end()
        || std::ranges::find(union_field.union_cases, value, &UnionCase::tag) != union_field.union_cases.end())
    {
        return fmt::format("case {} is not a registered type which fits in it, for a value of the tag (once per value)", type_name);
    }
    union_field.union_cases.push_back({ value, tag_value->name, case_type });
    return {};
}

std::string engine::type_builder_detail::find_duplicate_key(const TypeInfo& type)
{
    std::set<std::string_view> keys;
    for (const auto& field : type.fields)
    {
        if (field.kind != FieldKind::eUnion)
        {
            if (!keys.insert(field.name).second)
            {
                return fmt::format("field {} is written under the key of another field or union case", field.name);
            }
            continue;
        }
        for (const auto& c : field.union_cases)
        {
            if (!keys.insert(c.name).second)
            {
                return fmt::format("union {}: its case {} is written under the key of another field or union case", field.name, c.name);
            }
        }
    }
    return {};
}

const engine::UnionCase* engine::find_active_case(const TypeInfo& type, const FieldInfo& union_field, const std::byte* value)
{
    const auto* tag = type.find_field(union_field.union_tag);
    std::int32_t tag_value = 0;
    std::memcpy(&tag_value, value + tag->offset, sizeof(tag_value));
    const auto it = std::ranges::find(union_field.union_cases, tag_value, &UnionCase::tag);
    return it == union_field.union_cases.end() ? nullptr : &*it;
}

void engine::clear_union(const FieldInfo& union_field, std::byte* value)
{
    std::memset(value + union_field.offset, 0, union_field.size);
}

namespace
{
// the text of the engine_string_t* at `p`; null is the empty string
std::string_view engine_string_text(const std::byte* p)
{
    const engine_string_t* str = nullptr;
    std::memcpy(&str, p, sizeof(str));
    return str ? std::string_view(str->str) : std::string_view();
}

void own_struct_strings(const engine::TypeInfo& type, std::byte* value, engine::ValueBuffer& owner)
{
    using engine::FieldKind;
    for (const auto& field : type.fields)
    {
        if (field.kind == FieldKind::eUnion)
        {
            if (const auto* active = engine::find_active_case(type, field, value))
            {
                own_struct_strings(*active->type, value + field.offset, owner);
            }
            continue;
        }
        for (std::size_t i = 0; i < field.count; i++)
        {
            auto* p = value + field.offset + i * field.size;
            if (field.kind == FieldKind::eStruct)
            {
                own_struct_strings(*field.struct_type, p, owner);
            }
            else if (field.kind == FieldKind::eEngineString)
            {
                auto* copy = owner.make_string(engine_string_text(p));
                std::memcpy(p, &copy, sizeof(copy));
            }
        }
    }
}

bool equal_elements(const engine::FieldInfo& field, const std::byte* a, const std::byte* b)
{
    using engine::FieldKind;
    switch (field.kind)
    {
    case FieldKind::eString:
    {
        // the bytes after the terminator are no part of the value
        const auto* sa = reinterpret_cast<const char*>(a);
        const auto* sb = reinterpret_cast<const char*>(b);
        const auto length = strnlen(sa, field.size);
        return length == strnlen(sb, field.size) && std::memcmp(sa, sb, length) == 0;
    }
    case FieldKind::eEngineString: return engine_string_text(a) == engine_string_text(b);
    case FieldKind::eStruct: return engine::equal_values(*field.struct_type, a, b);
    case FieldKind::eUnion: return true;  // compared by equal_field, which knows its tag
    default: return std::memcmp(a, b, field.size) == 0;
    }
}
}  // namespace

bool engine::equal_field(const TypeInfo& type, const FieldInfo& field, const std::byte* a, const std::byte* b)
{
    if (field.kind == FieldKind::eUnion)
    {
        // equal tags select the same case, whose data is all the union holds
        const auto* tag = type.find_field(field.union_tag);
        if (tag && !equal_field(type, *tag, a, b))
        {
            return false;
        }
        const auto* active = find_active_case(type, field, a);
        return !active || equal_values(*active->type, a + field.offset, b + field.offset);
    }
    for (std::size_t i = 0; i < field.count; i++)
    {
        const auto offset = field.offset + i * field.size;
        if (!equal_elements(field, a + offset, b + offset))
        {
            return false;
        }
    }
    return true;
}

void engine::own_engine_strings(const TypeInfo& type, ValueBuffer& value)
{
    own_struct_strings(type, value.get(), value);
}

bool engine::holds_game_object(const TypeInfo& type)
{
    return std::ranges::any_of(type.fields, [](const FieldInfo& field) {
        return field.kind == FieldKind::eGameObject
            || (field.kind == FieldKind::eStruct && holds_game_object(*field.struct_type))
            || (field.kind == FieldKind::eUnion && std::ranges::any_of(field.union_cases, [](const UnionCase& c) { return holds_game_object(*c.type); }));
    });
}

bool engine::equal_values(const TypeInfo& type, const std::byte* a, const std::byte* b)
{
    return std::ranges::all_of(type.fields, [&](const FieldInfo& field) { return equal_field(type, field, a, b); });
}
