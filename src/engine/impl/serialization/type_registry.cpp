#include "type_registry.h"
#include "../logger.h"

#include <cstring>
#include <set>

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
