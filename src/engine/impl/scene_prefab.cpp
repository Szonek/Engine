#include "scene_prefab.h"

#include "asset_store.h"
#include "logger.h"
#include "profiler.h"
#include "scene.h"

#include <engine.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

std::unique_ptr<engine::ScenePrefab> engine::ScenePrefab::load(std::string_view asset_id)
{
    const auto file_path = AssetStore::get_instance().get_scenes_base_path() / asset_id;
    std::ifstream file(file_path, std::ios::binary);
    if (!file)
    {
        log::log(log::LogLevel::eError, fmt::format("Scene prefab {}: can't read {}.\n", asset_id, file_path.string()));
        return nullptr;
    }
    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // logs why it can't be used
    if (!check_scene_file(json, asset_id))
    {
        return nullptr;
    }
    return std::unique_ptr<ScenePrefab>(new ScenePrefab(asset_id, std::move(json)));
}

engine::ScenePrefab::ScenePrefab(std::string_view name, std::string&& json)
    : name_(name)
    , json_(std::move(json))
{
}

std::vector<engine::PrefabInstanceNode> engine::ScenePrefab::instantiate(Scene& scene, Instance instance, entt::entity root, const EntityHandles* node_handles,
    const InstanceIds* ids) const
{
    ENGINE_PROFILE_SECTION;
    // its file is being loaded already (the level, or a scene prefab's instance this one is placed in)
    if (scene.prefabs().is_file_loading(name_))
    {
        log::log(log::LogLevel::eError, fmt::format("Scene prefab {} places itself (directly or through other scene files), the instance inside itself is "
            "not created.\n", name_));
        return {};
    }
    if (root == entt::null)
    {
        root = instance == Instance::eTransient ? scene.create_transient_entity() : scene.create_new_entity();
        engine_name_component_t name{};
        std::strncpy(name.name, std::filesystem::path(name_).stem().string().c_str(), std::size(name.name) - 1);
        scene.add_component<engine_name_component_t>(root);
        scene.update_component(root, name);
        scene.add_component<engine_transform_component_t>(root);
    }

    SceneInstanceTarget target;
    target.root = root;
    target.ids = ids ? *ids : InstanceIds{ scene.get_persistent_id(root), {} };
    if (instance == Instance::eTransient)
    {
        target.ids.root_id = EntityIdRegistry::INVALID_ID;
    }
    target.node_handles = node_handles;
    SceneLoadOptions options;
    options.instance = &target;
    // the file was checked when it was read, so it loads
    scene.load(json_, name_, options);

    std::vector<PrefabInstanceNode> nodes;
    nodes.reserve(target.nodes.size() + 1);
    nodes.push_back({ "", root, {} });
    std::move(target.nodes.begin(), target.nodes.end(), std::back_inserter(nodes));
    return nodes;
}
