#include "prefab_system.h"

#include "application.h"
#include "logger.h"
#include "profiler.h"
#include "components_internals/prefab_instance_component.h"
#include "serialization/scene_assets.h"

#include <fmt/format.h>

#include <utility>

engine::PrefabSystem::PrefabSystem(Application& app, Scene& scene, entt::registry& registry, SceneAssets& assets)
    : app_(app)
    , scene_(scene)
    , registry_(registry)
    , assets_(assets)
{
}

entt::entity engine::PrefabSystem::instantiate_prefab(std::uint32_t prefab, Prefab::Instance instance, entt::entity root, const EntityHandles* node_handles)
{
    ENGINE_PROFILE_SECTION;
    const auto* typed_prefab = app_.get_prefab(prefab);
    if (!typed_prefab)
    {
        log::log(log::LogLevel::eError, fmt::format("Can't instantiate prefab {}: it doesn't exist.\n", prefab));
        return entt::null;
    }
    PrefabInstanceInfo info{ typed_prefab->get_name(), typed_prefab->instantiate(scene_, instance, root, node_handles) };
    if (info.nodes.empty())
    {
        return entt::null;
    }
    const auto root_entity = info.nodes.front().entity;
    if (instance == Prefab::Instance::ePersistent)
    {
        capture_prefab_components(app_.get_type_registry(), registry_, info);
    }
    registry_.emplace<prefab_instance_internal_component_t>(root_entity, app_, prefab, std::move(info));
    return root_entity;
}

const engine::PrefabInstanceInfo* engine::PrefabSystem::find_instance(entt::entity root) const
{
    const auto* component = registry_.try_get<prefab_instance_internal_component_t>(root);
    return component ? &component->get_info() : nullptr;
}

const engine::PrefabInstanceInfo* engine::PrefabSystem::instantiate(std::string_view prefab_asset_id, entt::entity root, const EntityHandles& node_handles)
{
    const auto prefab = assets_.resolve_prefab(prefab_asset_id);
    if (prefab == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return nullptr;
    }
    const auto entity = instantiate_prefab(prefab, Prefab::Instance::ePersistent, root, &node_handles);
    return entity == entt::null ? nullptr : find_instance(entity);
}
