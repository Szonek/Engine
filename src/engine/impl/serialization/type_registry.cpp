#include "type_registry.h"
#include "../logger.h"

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
    type.handle = static_cast<std::uint32_t>(types_.size());
    types_.push_back(std::make_unique<TypeInfo>(std::move(type)));
    return types_.back().get();
}

const engine::TypeInfo* engine::TypeRegistry::find(std::string_view name) const
{
    const auto it = std::ranges::find_if(types_, [name](const auto& t) { return t->name == name; });
    return it == types_.end() ? nullptr : it->get();
}

const engine::TypeInfo* engine::TypeRegistry::get(std::uint32_t handle) const
{
    return handle < types_.size() ? types_[handle].get() : nullptr;
}

std::vector<const engine::TypeInfo*> engine::TypeRegistry::get_components() const
{
    std::vector<const TypeInfo*> ret;
    for (const auto& t : types_)
    {
        if (t->is_component())
        {
            ret.push_back(t.get());
        }
    }
    return ret;
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
