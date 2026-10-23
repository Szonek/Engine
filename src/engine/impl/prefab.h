#pragma once
#include "mesh_defs.h"
#include "serialization/prefab_instances.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace engine
{
class Application;
class Scene;

// A model (glTF) loaded into an application, named by its path relative to the models folder ("characters/Rogue.glb").
// It holds a reference to each asset created from the model's data (geometries, textures, skin, animation set; the
// model's asset IDs are "<path>#<name>", see asset_id.h) and releases them when destroyed, so they live as long as the
// prefab. An image in its own file is an asset of its own which other models (or the game) may use too: the prefab
// only holds one reference to it. instantiate() stamps out the model's node hierarchy as game objects that use the
// assets.
class Prefab
{
public:
    // Creates the model's assets in `app`, which must outlive the prefab. Returns nullptr (and logs why) when one of
    // them can't be created; the ones created so far are released.
    static std::unique_ptr<Prefab> create(Application& app, ModelDesc&& model);

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;
    Prefab(Prefab&&) = delete;
    Prefab& operator=(Prefab&&) = delete;
    ~Prefab();

    const std::string& get_name() const { return model_.source_name; }

    enum class Instance
    {
        ePersistent,  // every game object gets a persistent ID
        eTransient    // no game object gets one (Scene::create_transient_entity)
    };

    // Creates the game objects of the model in `scene` and returns them with their node paths, the root first (the
    // model's node without a parent; its path is ""), or nothing (logged) when the model has no root. The root is the
    // parent of all the others, so the model's hierarchy is flat. The nodes which are joints of the skin are not
    // created: the skin owner game object carries the skin, and nodes attached to a joint follow it.
    // `root`, when given, is the game object which becomes the root (a scene file's, with the ID of the file). In a
    // persistent instance the other game objects get the hashed IDs of their node paths (EntityIdRegistry::hashed_id):
    // a node's path is the names of the nodes from the root down, joined by '/'; a name which is empty or used by a
    // sibling before it gets the node's position among its siblings ("Arm#1").
    std::vector<PrefabInstanceNode> instantiate(Scene& scene, Instance instance, entt::entity root = entt::null) const;

private:
    Prefab(Application& app, ModelDesc&& model);

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
