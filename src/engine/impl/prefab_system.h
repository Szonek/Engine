#pragma once
#include "prefab.h"
#include "serialization/prefab_instances.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace engine
{
class Application;
class Scene;
class SceneAssets;

// The prefab instances of a scene: instantiates prefabs into it, marking each root with its prefab instance component
// (the instance's reference to its prefab and what scene files need about it), and gives scene files access to the
// instances (IPrefabInstances).
class PrefabSystem : public IPrefabInstances
{
public:
    // `assets` loads and holds the prefabs scene files name
    PrefabSystem(Application& app, Scene& scene, entt::registry& registry, SceneAssets& assets);
    PrefabSystem(const PrefabSystem&) = delete;
    PrefabSystem& operator=(const PrefabSystem&) = delete;

    // Instantiates the prefab (see Prefab::instantiate) and marks the root with its prefab instance component.
    // entt::null when it fails (logged).
    entt::entity instantiate_prefab(std::uint32_t prefab, Prefab::Instance instance, entt::entity root = entt::null, const EntityHandles* node_handles = nullptr,
        const InstanceIds* ids = nullptr);

    // IPrefabInstances; a returned instance is valid until the next prefab is instantiated
    const PrefabInstanceInfo* find_instance(entt::entity root) const override;
    const PrefabInstanceInfo* instantiate(std::string_view prefab_asset_id, entt::entity root, const EntityHandles& node_handles, const InstanceIds& ids) override;

    // The scene files being loaded into the scene (Scene::load, nested while a scene prefab's file loads), by name: a
    // path under assets/scenes or absolute. A scene prefab whose file is one of them would be placed inside itself.
    void begin_file_load(std::string_view file_name);
    void end_file_load();
    bool is_file_loading(std::string_view file_name) const;

private:
    Application& app_;
    Scene& scene_;
    entt::registry& registry_;
    SceneAssets& assets_;
    std::vector<std::filesystem::path> loading_files_;
};
}  // namespace engine
