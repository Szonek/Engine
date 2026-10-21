#pragma once
#include <engine.h>

#include "physics_world.h"
#include "world_transform.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <array>
#include <deque>
#include <functional>
#include <span>
#include <tuple>
#include <vector>

namespace engine
{
// The physics of a scene: keeps a body in the PhysicsWorld for every collider and character controller of the registry
// and answers physics queries. Like Unity, bodies are created as soon as their components exist (and again when they
// change), and moved transforms are synced to the bodies lazily: before the physics step and before every query, so
// queries during the scripts' update see this frame's state.
class PhysicsSystem
{
public:
    // `joint_matrix` gives the model matrix of a joint attachment's joint (the scene's skin poses); `is_editing` is true
    // while the editor edits the scene (moving static colliders is on purpose then). Connects to the registry's signals:
    // construct it before the component initializers are connected, so its listeners run after them.
    PhysicsSystem(entt::registry& registry, JointMatrixFunction joint_matrix, std::function<bool()> is_editing);
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    ~PhysicsSystem();

    // syncs the bodies to the transforms, applies the forces, steps the simulation and writes the dynamic bodies back
    // to their transforms; dt in milliseconds
    void update(float dt);

    // see engineCharacterControllerMove; dt in milliseconds
    engine_result_code_t move_character(entt::entity entity, const glm::vec3& velocity, float dt, engine_character_controller_move_result_t& out_result);
    engine_ray_hit_info_t raycast(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance);
    // applied with the next update (a body created this frame isn't simulated yet); false without a body or an unknown type
    bool add_force(entt::entity entity, std::array<float, 3> force, engine_force_type_t type);
    void set_gravity(std::array<float, 3> gravity);
    // the contacts of the last step
    const std::vector<CollisionDesc>& get_collisions() const;

    void enable_debug_draw(bool enable);
    bool is_debug_draw_enabled() const;
    void collect_debug_lines(std::vector<DebugLineGpuData>& out_lines);

private:
    // true when the entity's world transform depends on other game objects (a parent or joint attachment)
    bool follows_other_game_objects(entt::entity entity) const;
    // the type of the entity's body: its rigid body's, or its collider's alone
    PhysicsWorld::BodyType get_body_type(entt::entity entity) const;
    // a collider without a rigid body: kinematic when it follows other game objects, otherwise static
    PhysicsWorld::BodyType get_collider_body_type(entt::entity entity) const;
    // the entity's world matrix from the current transforms (the world transform components are from the last update)
    glm::mat4 get_world_matrix(entt::entity entity) const;
    // (re)creates the entity's body (character controller, or collider with its rigid body) at its current world
    // transform (none without a transform); the flags leave out a component which is being removed
    void create_body(entt::entity entity, bool without_rigid_body = false, bool without_character_controller = false);
    // moves the bodies to their game objects' current world transforms; without `force` only when a transform, parent or
    // joint attachment changed since the last sync (before queries)
    void sync_bodies_to_transforms(bool force);
    void apply_forces();
    void write_dynamic_bodies_to_transforms();

    void on_body_component_changed(entt::registry& registry, entt::entity entity);
    void on_rigid_body_removed(entt::registry& registry, entt::entity entity);
    void on_character_controller_removed(entt::registry& registry, entt::entity entity);
    void on_transforms_changed(entt::registry& registry, entt::entity entity);

private:
    entt::registry& registry_;
    JointMatrixFunction joint_matrix_;
    std::function<bool()> is_editing_;
    PhysicsWorld world_;
    // bodies whose game object's transform changed since the last sync
    entt::observer moved_bodies_observer_;
    // reported moves of static bodies skip these: being placed in the frame they were created is no move
    std::vector<entt::entity> bodies_created_this_frame_;
    // a transform, parent or joint attachment changed since the bodies were last synced to the transforms
    bool transforms_changed_ = false;
    std::deque<std::tuple<entt::entity, std::array<float, 3>, engine_force_type_t>> forces_to_apply_;
};
}  // namespace engine
