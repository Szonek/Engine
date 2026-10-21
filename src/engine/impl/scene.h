#pragma once
#include <engine.h>
#include "graphics.h"

#include "physics_world.h"
#include "entity_id_registry.h"

#include "render/render_scene_data.h"

#include <entt/entt.hpp>

#include <array>
#include <deque>
#include <optional>
#include <span>
#include <tuple>

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

    void enable_physics_debug_draw(bool enable);
    bool is_physics_debug_draw_enabled() const;
    void enable_nav_mesh_debug_draw(bool enable);
    bool is_nav_mesh_debug_draw_enabled() const;
    engine_result_code_t update(float dt);

    // every entity gets a persistent ID (see EntityIdRegistry), except the reserved ENGINE_INVALID_GAME_OBJECT_ID one
    entt::entity create_new_entity();
    // restores an entity with the given persistent ID (e.g. undo of a destroy), or without one when it is
    // EntityIdRegistry::INVALID_ID (the entity was transient); the hint is the entt value to reuse if possible
    entt::entity create_new_entity(entt::entity hint, std::uint64_t persistent_id);
    // an entity without a persistent ID, for objects that are not part of the scene content (e.g. the editor camera)
    entt::entity create_transient_entity();
    // destroys the entity and its children, with theirs
    void destroy_entity(entt::entity entity);
    bool is_valid_entity(entt::entity entity) const;

    // the space the entity's transform is local to (parent world * joint), see engine::get_parent_space_matrix
    glm::mat4 get_parent_space_matrix(entt::entity entity) const;

    // see engineCharacterControllerMove; dt in milliseconds
    engine_result_code_t move_character_controller(entt::entity entity, const glm::vec3& velocity, float dt, engine_character_controller_move_result_t& out_result);

    // the entities whose parent component points at `entity`, in the order they were parented; invalidated by any
    // change of the hierarchy
    std::span<const entt::entity> get_children(entt::entity entity) const;

    // the engine-owned animation controller of the entity, or nullptr
    AnimationController* get_animation_controller(entt::entity entity);

    // EntityIdRegistry::INVALID_ID for an entity without a persistent ID
    std::uint64_t get_persistent_id(entt::entity entity) const;

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

    // (Re)builds the nav mesh from the static colliders (see engine_nav_modifier_component_t) with the settings of the
    // scene's nav surface component, and keeps it on that game object. Fails when there is no nav surface or nothing
    // is walkable (the previous nav mesh is dropped then).
    engine_result_code_t build_nav_mesh();
    // the nav mesh of the scene's nav surface, or nullptr when it hasn't been built
    const NavMesh* get_nav_mesh() const;

    void set_physcis_gravity(std::array<float, 3> g);
    const std::vector<CollisionDesc>& get_physcis_collisions() const;

    engine_ray_hit_info_t raycast_into_physics_world(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance);
    bool add_force_to_physics_entity(entt::entity entity, std::array<float, 3> force, engine_force_type_t type);

    glm::vec3 convert_world_point_to_screen_point(const glm::vec3& world_point, engine_game_object_t camera_go);
    glm::vec3 convert_screen_point_to_world_point(glm::vec3 screen_point, engine_game_object_t camera_go);
    glm::mat4 get_camera_view(entt::entity camera_go);
    glm::mat4 get_camera_projection(entt::entity camera_go);

private:
    engine_result_code_t physics_update(float dt);
    // true when the entity's world transform depends on other game objects (a parent or joint attachment)
    bool follows_other_game_objects(entt::entity entity) const;
    // the type of the entity's body: its rigid body's, or for a collider alone kinematic when it follows other game objects
    PhysicsWorld::BodyType get_body_type(entt::entity entity) const;
    // (re)creates the entity's body (collider or character controller) at its world transform
    void create_body(entt::entity entity);
    // whether colliders, rigid bodies or character controllers were added or changed since their bodies were made
    bool has_pending_bodies() const;
    void create_pending_bodies();
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
    entt::registry entity_registry_;
    // declared after the registry: it disconnects from the registry's signals when destroyed
    EntityIdRegistry entity_ids_;
    entt::observer mesh_update_observer;
    entt::observer collider_create_observer;
    entt::observer collider_update_observer;
    entt::observer transform_update_collider_observer;
    entt::observer rigid_body_create_observer;
    entt::observer rigid_body_update_observer;
    entt::observer character_controller_create_observer;
    entt::observer character_controller_update_observer;
    // reported moves of static bodies skip these: being placed in the frame they were created is no move
    std::vector<entt::entity> bodies_created_this_frame_;

    PhysicsWorld physics_world_;
    bool nav_mesh_debug_draw_ = false;
    std::deque<std::tuple<entt::entity, std::array<float, 3>, engine_force_type_t>> forces_to_apply_;
};
}  // namespace engine