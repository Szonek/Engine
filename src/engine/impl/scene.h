#pragma once
#include <engine.h>
#include "graphics.h"

#include "physics_world.h"
#include "entity_id_registry.h"

#include "render/render_scene_data.h"

#include <entt/entt.hpp>

#include <array>
#include <deque>
#include <tuple>

namespace engine
{
class Application;
class Renderer;
class Skin;
struct SkinPose;
class Scene
{
public:
    Scene(Application* app, RenderContext& rdx, Renderer& renderer, const engine_scene_create_desc_t& config, engine_result_code_t& out_code);
    Scene(const Scene&) = delete;
    Scene(Scene&& rhs) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&& rhs) = delete;
    ~Scene();

    Application* get_application() const;

    void enable_physics_debug_draw(bool enable);
    bool is_physics_debug_draw_enabled() const;
    engine_result_code_t update(float dt);

    // every entity gets a persistent ID (see EntityIdRegistry), except the reserved ENGINE_INVALID_GAME_OBJECT_ID one
    entt::entity create_new_entity();
    // restores an entity with the given persistent ID (e.g. undo of a destroy), or without one when it is
    // EntityIdRegistry::INVALID_ID (the entity was transient); the hint is the entt value to reuse if possible
    entt::entity create_new_entity(entt::entity hint, std::uint64_t persistent_id);
    // an entity without a persistent ID, for objects that are not part of the scene content (e.g. the editor camera)
    entt::entity create_transient_entity();
    void destroy_entity(entt::entity entity);
    bool is_valid_entity(entt::entity entity) const;

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
    // local_to_world of every transform: TRS, then joint attachments, then the parent chain
    void update_world_transforms();
    // (re)creates the pose of a skin instance when its skin component gets a skin, removes it when it loses one
    void on_skin_component_changed(entt::registry& registry, entt::entity entity);
    // the pose and skin asset of the skin instance owned by `skin_owner`, or {nullptr, nullptr} (both or neither)
    std::pair<const SkinPose*, const Skin*> get_skin_instance(engine_game_object_t skin_owner) const;
    // Collects cameras and everything visible in this scene into render_data and submits it to the renderer. No rendering API calls happen here.
    void submit_render_data(SceneRenderData&& render_data);

private:
    Application* app_;
    RenderContext& rdx_;
    Renderer& renderer_;
    entt::registry entity_registry_;
    // declared after the registry: it disconnects from the registry's signals when destroyed
    EntityIdRegistry entity_ids_;
    entt::observer transform_model_matrix_update_observer;
    entt::observer mesh_update_observer;
    entt::observer collider_create_observer;
    entt::observer collider_update_observer;
    entt::observer transform_update_collider_observer;
    entt::observer rigid_body_create_observer;
    entt::observer rigid_body_update_observer;

    PhysicsWorld physics_world_;
    std::deque<std::tuple<entt::entity, std::array<float, 3>, engine_force_type_t>> forces_to_apply_;
};
}  // namespace engine