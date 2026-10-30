#pragma once
#include <engine.h>
#include "graphics.h"

#include "physics_system.h"
#include "entity_id_registry.h"
#include "prefab_system.h"
#include "serialization/scene_assets.h"
#include "serialization/scene_serializer.h"

#include "render/render_scene_data.h"

#include <entt/entt.hpp>

#include <optional>
#include <span>

namespace engine
{
class Application;
class Renderer;
class Skin;
struct SkinPose;
class AnimationController;
class NavMesh;
class Scene
{
public:
    Scene(Application* app, Renderer& renderer, const engine_scene_create_desc_t& config, engine_result_code_t& out_code);
    Scene(const Scene&) = delete;
    Scene(Scene&& rhs) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&& rhs) = delete;
    ~Scene();

    Application* get_application() const;
    // the scene's bodies, physics queries and character controllers
    PhysicsSystem& physics();
    const PhysicsSystem& physics() const;
    // instantiates prefabs and knows the scene's prefab instances
    PrefabSystem& prefabs();
    const PrefabSystem& prefabs() const;
    // the assets the scene holds for what scene files and the editor placed in it, by asset ID
    SceneAssets& assets();

    void enable_nav_mesh_debug_draw(bool enable);
    bool is_nav_mesh_debug_draw_enabled() const;
    engine_result_code_t update(float dt);

    // Entity 0 has the value of ENGINE_INVALID_GAME_OBJECT_ID, so every scene reserves it when created: it has no
    // components, is never destroyed and is not valid for anyone (is_valid_entity, the C API, the editor).
    static constexpr entt::entity RESERVED_ENTITY{ ENGINE_INVALID_GAME_OBJECT_ID };

    // every entity gets a persistent ID (see EntityIdRegistry)
    entt::entity create_new_entity();
    // an entity without a persistent ID, for objects that are not part of the scene content (e.g. the editor camera)
    entt::entity create_transient_entity();
    // a game object of a prefab instance whose root has the persistent ID `root_id`: it gets the hashed ID of its node
    // path (EntityIdRegistry::hashed_id), or a new counter ID (logged) when another game object has that one
    // `node_handles`: game objects created already without a persistent ID (a scene file load restoring handles); the
    // one listed under the node's hashed ID becomes the node instead of a new one
    entt::entity create_prefab_node_entity(std::uint64_t root_id, std::string_view node_path, const EntityHandles* node_handles = nullptr);
    // destroys the entity and its children, with theirs
    void destroy_entity(entt::entity entity);
    bool is_valid_entity(entt::entity entity) const;

    // the space the entity's transform is local to (parent world * joint), see engine::get_parent_space_matrix
    glm::mat4 get_parent_space_matrix(entt::entity entity) const;
    // the entity's world matrix from the current transforms (the world transform component is from the last update),
    // see engine::compute_world_matrix
    glm::mat4 compute_world_matrix(entt::entity entity) const;

    // the entities whose parent component points at `entity`, in the order they were parented; invalidated by any
    // change of the hierarchy
    std::span<const entt::entity> get_children(entt::entity entity) const;
    // the entity and everything parented under it, the entity first
    std::vector<entt::entity> get_subtree(entt::entity entity) const;
    // moves a child of `parent` to `index` in its children (e.g. back where it was after an undo); the end when it is
    // past it
    void set_child_index(entt::entity parent, entt::entity child, std::size_t index);

    // the engine-owned animation controller of the entity, or nullptr
    AnimationController* get_animation_controller(entt::entity entity);

    // EntityIdRegistry::INVALID_ID for an entity without a persistent ID
    std::uint64_t get_persistent_id(entt::entity entity) const;

    // the scene as a scene file, see engine::save_scene
    std::string save() const;
    // only these entities as a scene file, see engine::save_game_objects
    std::string save(std::span<const entt::entity> entities) const;
    // adds the game objects of a scene file to the scene, see engine::load_scene; the assets it uses live as long as
    // the scene (SceneAssets)
    bool load(std::string_view json, std::string_view file_name, const SceneLoadOptions& options = {});

    entt::runtime_view create_runtime_view();

    std::vector<entt::entity> get_all_entities() const;

    template<typename T>
    void attach_component_to_runtime_view(entt::runtime_view& rv)
    {
        rv.iterate(entity_registry_.storage<T>());
    }

    template<typename T>
    T* add_component(entt::entity entity, T&& component)
    {
        return &entity_registry_.emplace<T>(entity, std::move(component));
    }

    template<typename T>
    T* add_component(entt::entity entity)
    {
        return &entity_registry_.emplace<T>(entity);
    }

    template<typename T>
    const T* get_component(entt::entity entity) const
    {
        return &entity_registry_.get<T>(entity);
    }

    template<typename T>
    T* get_component(entt::entity entity)
    {
        return &entity_registry_.get<T>(entity);
    }

    template<typename T>
    void patch_component(entt::entity entity, std::function<void(T&)>&& func)
    {
        entity_registry_.patch<T>(entity, std::move(func));
    }

    template<typename T>
    void update_component(entt::entity entity, const T& t)
    {
        //entity_registry_.patch<T>(entity, [&t](auto& p) { [p = t; ] });
        entity_registry_.replace<T>(entity, t);
    }

    template<typename T>
    void remove_component(entt::entity entity)
    {
        //ToDo: use .remove(entity) instead of erase?
        entity_registry_.erase<T>(entity);
    }

    template<typename T>
    bool has_component(entt::entity entity) const
    {
        return entity_registry_.any_of<T>(entity);
    }

    // Components of a registered type without knowing it (game-defined components, the editor's generic inspector),
    // through its ComponentStorage: values are pointers to a value of the type, added with its defaults.
    bool has_component(const TypeInfo& type, entt::entity entity) const { return type.storage->has(entity_registry_, entity); }
    void add_component(const TypeInfo& type, entt::entity entity) { type.storage->add(entity_registry_, entity); }
    void get_component(const TypeInfo& type, entt::entity entity, void* out) const { type.storage->get(entity_registry_, entity, out); }
    void update_component(const TypeInfo& type, entt::entity entity, const void* value) { type.storage->update(entity_registry_, entity, value); }
    void remove_component(const TypeInfo& type, entt::entity entity) { type.storage->remove(entity_registry_, entity); }
    // game-defined types only (see ComponentStorage::pool and change_count)
    std::uint64_t get_change_count(const TypeInfo& type) const { return type.storage->change_count ? type.storage->change_count(entity_registry_) : 0; }
    void attach_component_to_runtime_view(const TypeInfo& type, entt::runtime_view& rv)
    {
        if (type.storage->pool)
        {
            rv.iterate(type.storage->pool(entity_registry_));
        }
    }

    // (Re)builds the nav mesh from the static colliders (see engine_nav_modifier_component_t) with the settings of the
    // scene's nav surface component, and keeps it on that game object. Fails when there is no nav surface or nothing
    // is walkable (the previous nav mesh is dropped then).
    engine_result_code_t build_nav_mesh();
    // the nav mesh of the scene's nav surface, or nullptr when it hasn't been built
    const NavMesh* get_nav_mesh() const;

    glm::vec3 convert_world_point_to_screen_point(const glm::vec3& world_point, engine_game_object_t camera_go);
    glm::vec3 convert_screen_point_to_world_point(glm::vec3 screen_point, engine_game_object_t camera_go);
    glm::mat4 get_camera_view(entt::entity camera_go);
    glm::mat4 get_camera_projection(entt::entity camera_go);

private:
    // the world_transform_internal_component_t of every transform, see engine::update_world_transforms
    void update_world_transforms();
    // the model matrix of the attachment's joint in its skin instance, nullopt (logged) when it has none
    std::optional<glm::mat4> get_joint_matrix(const engine_joint_attachment_component_t& joint_attachment) const;
    // (re)creates the pose of a skin instance when its skin component gets a skin, removes it when it loses one
    void on_skin_component_changed(entt::registry& registry, entt::entity entity);
    // (re)creates the entity's animation controller from its animation set and skin, or removes it when either is missing
    void on_animation_controller_component_changed(entt::registry& registry, entt::entity entity);
    // the nav modifier of the entity or its closest ancestor with one, or nullptr
    const engine_nav_modifier_component_t* find_nav_modifier(entt::entity entity) const;
    // the pose and skin asset of the skin instance owned by `skin_owner`, or {nullptr, nullptr} (both or neither)
    std::pair<const SkinPose*, const Skin*> get_skin_instance(engine_game_object_t skin_owner) const;
    // Collects cameras and everything visible in this scene into render_data and submits it to the renderer. No rendering API calls happen here.
    void submit_render_data(SceneRenderData&& render_data);

private:
    Application* app_;
    Renderer& renderer_;
    // declared before the registry: the assets scene files loaded are released after the game objects using them
    SceneAssets assets_;
    entt::registry entity_registry_;
    // declared after the registry: it disconnects from the registry's signals when destroyed
    EntityIdRegistry entity_ids_;
    // declared after the registry too; constructed before the constructor connects the component initializers
    PhysicsSystem physics_;
    PrefabSystem prefabs_;

    bool nav_mesh_debug_draw_ = false;
};
}  // namespace engine