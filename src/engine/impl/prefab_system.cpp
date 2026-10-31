#include "prefab_system.h"

#include "application.h"
#include "asset_store.h"
#include "logger.h"
#include "profiler.h"
#include "components_internals/prefab_instance_component.h"
#include "serialization/scene_assets.h"

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <utility>

engine::PrefabSystem::PrefabSystem(Application& app, Scene& scene, entt::registry& registry, SceneAssets& assets)
    : app_(app)
    , scene_(scene)
    , registry_(registry)
    , assets_(assets)
{
}

entt::entity engine::PrefabSystem::instantiate_prefab(std::uint32_t prefab, Prefab::Instance instance, entt::entity root, const EntityHandles* node_handles,
    const InstanceIds* ids)
{
    ENGINE_PROFILE_SECTION;
    const auto* typed_prefab = app_.get_prefab(prefab);
    if (!typed_prefab)
    {
        log::log(log::LogLevel::eError, fmt::format("Can't instantiate prefab {}: it doesn't exist.\n", prefab));
        return entt::null;
    }
    PrefabInstanceInfo info{ typed_prefab->get_name(), typed_prefab->instantiate(scene_, instance, root, node_handles, ids) };
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

const engine::PrefabInstanceInfo* engine::PrefabSystem::instantiate(std::string_view prefab_asset_id, entt::entity root, const EntityHandles& node_handles,
    const InstanceIds& ids)
{
    const auto prefab = assets_.resolve_prefab(prefab_asset_id);
    if (prefab == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return nullptr;
    }
    const auto instance = ids.root_id == EntityIdRegistry::INVALID_ID ? Prefab::Instance::eTransient : Prefab::Instance::ePersistent;
    const auto entity = instantiate_prefab(prefab, instance, root, &node_handles, &ids);
    return entity == entt::null ? nullptr : find_instance(entity);
}

namespace
{
// a scene file's name as one path, however it was written ("rooms/a.json", "rooms/../rooms/a.json" or absolute)
std::filesystem::path scene_file_key(std::string_view file_name)
{
    return (engine::AssetStore::get_instance().get_scenes_base_path() / file_name).lexically_normal();
}
}  // namespace

void engine::PrefabSystem::begin_file_load(std::string_view file_name)
{
    loading_files_.push_back(scene_file_key(file_name));
}

void engine::PrefabSystem::end_file_load()
{
    loading_files_.pop_back();
}

bool engine::PrefabSystem::is_file_loading(std::string_view file_name) const
{
    return std::ranges::find(loading_files_, scene_file_key(file_name)) != loading_files_.end();
}

void engine::PrefabSystem::add_scene_file(std::string_view file_name)
{
    if (!is_scene_file(file_name))
    {
        scene_files_.push_back(scene_file_key(file_name));
    }
}

bool engine::PrefabSystem::is_scene_file(std::string_view file_name) const
{
    return std::ranges::find(scene_files_, scene_file_key(file_name)) != scene_files_.end();
}

void engine::PrefabSystem::clear_scene_files()
{
    scene_files_.clear();
}

bool engine::PrefabSystem::places_scene_file(std::string_view file_name) const
{
    const auto key = scene_file_key(file_name);
    // the roots of nested instances are marked too, so every file placed directly or not is found
    for (const auto& [entity, instance] : registry_.view<const prefab_instance_internal_component_t>().each())
    {
        const auto& prefab = instance.get_info().prefab;
        if (is_scene_prefab_id(prefab) && scene_file_key(prefab) == key)
        {
            return true;
        }
    }
    return false;
}

engine::prefab_instance_internal_component_t::prefab_instance_internal_component_t(Application& app, std::uint32_t prefab, PrefabInstanceInfo&& info)
    : app_(&app)
    , prefab_(prefab)
    , info_(std::move(info))
{
    const auto acquired = app_->acquire_prefab(prefab_);
    assert(acquired && "the prefab of an instance must be valid");
    if (!acquired)
    {
        app_ = nullptr;
    }
}

engine::prefab_instance_internal_component_t::prefab_instance_internal_component_t(prefab_instance_internal_component_t&& rhs) noexcept
    : app_(std::exchange(rhs.app_, nullptr))
    , prefab_(rhs.prefab_)
    , info_(std::move(rhs.info_))
{
}

engine::prefab_instance_internal_component_t& engine::prefab_instance_internal_component_t::operator=(prefab_instance_internal_component_t&& rhs) noexcept
{
    if (this != &rhs)
    {
        release();
        app_ = std::exchange(rhs.app_, nullptr);
        prefab_ = rhs.prefab_;
        info_ = std::move(rhs.info_);
    }
    return *this;
}

engine::prefab_instance_internal_component_t::~prefab_instance_internal_component_t()
{
    release();
}

void engine::prefab_instance_internal_component_t::release()
{
    if (app_)
    {
        std::exchange(app_, nullptr)->release_prefab(prefab_);
    }
}
