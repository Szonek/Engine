#pragma once
#include "mesh_defs.h"
#include "prefab.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace engine
{
class Application;

// A model (glTF) loaded into an application, named by its path relative to the models folder ("characters/Rogue.glb").
// It holds a reference to each asset created from the model's data (geometries, textures, skin, animation set; the
// model's asset IDs are "<path>#<name>", see asset_id.h) and releases them when destroyed, so they live as long as the
// prefab. An image in its own file is an asset of its own which other models (or the game) may use too: the prefab
// only holds one reference to it. instantiate() stamps out the model's node hierarchy as game objects that use the
// assets.
class ModelPrefab : public Prefab
{
public:
    // Creates the model's assets in `app`, which must outlive the prefab. Returns nullptr (and logs why) when one of
    // them can't be created; the ones created so far are released.
    static std::unique_ptr<ModelPrefab> create(Application& app, ModelDesc&& model);

    ~ModelPrefab() override;

    const std::string& get_name() const override { return model_.source_name; }

    // The root is the model's node without a parent (nothing, logged, when it has none) and the parent of all the
    // others, so the model's hierarchy is flat. The nodes which are joints of the skin are not created: the skin owner
    // game object carries the skin, and nodes attached to a joint follow it. A node's path is the names of the nodes
    // from the root down, joined by '/'; a name which is empty or used by a sibling before it gets the node's position
    // among its siblings ("Arm#1").
    std::vector<PrefabInstanceNode> instantiate(Scene& scene, Instance instance, entt::entity root = entt::null, const EntityHandles* node_handles = nullptr,
        const InstanceIds* ids = nullptr) const override;

private:
    ModelPrefab(Application& app, ModelDesc&& model);

    bool create_assets();
    bool create_textures();
    void create_materials();
    bool create_skin();
    bool create_animation_set();

    const ModelNodeDesc* find_node(const std::string& name) const;

    Application& app_;
    ModelDesc model_;
    std::vector<std::uint32_t> geometries_;
    std::vector<std::uint32_t> textures_;
    std::vector<engine_material_component_t> materials_;
    // a model has at most one skin
    std::uint32_t skin_ = ENGINE_INVALID_OBJECT_HANDLE;
    std::uint32_t animation_set_ = ENGINE_INVALID_OBJECT_HANDLE;
    // node index -> its path
    std::map<std::int32_t, std::string> node_paths_;
};
}  // namespace engine
