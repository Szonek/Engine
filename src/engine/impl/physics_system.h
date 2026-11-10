#pragma once
#include <engine.h>

#include "physics_world.h"
#include "world_transform.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

namespace engine
{
// The physics of a scene: keeps a body in the PhysicsWorld for every collider and character controller of the registry
// and answers physics queries. Like Unity, bodies are created as soon as their components exist (and again when they
// change), and moved transforms are synced to the bodies lazily: before the physics step and before every query, so
// queries during the scripts' update see this frame's state.
// A heightfield collider is a terrain, which never moves: its body is static whatever rigid body its game object has.
// The one place saying so, for the physics and for what reads colliders like it (nav mesh builds).
inline bool is_always_static(const engine_collider_component_t& collider)
{
    return collider.type == ENGINE_COLLIDER_TYPE_HEIGHTFIELD;
}

class PhysicsSystem
{
public:
    // the heights of a terrain asset and their revision (Terrain::get_revision); no data for a terrain which doesn't exist
    struct TerrainHeights
    {
        std::shared_ptr<const TerrainData> data;
        std::uint64_t revision = 0;
    };
    using TerrainLookup = std::function<TerrainHeights(engine_terrain_t)>;

    // `joint_matrix` gives the model matrix of a joint attachment's joint (the scene's skin poses); `is_editing` is true
    // while the editor edits the scene (moving static colliders is on purpose then); `terrain_lookup` gives the heights
    // of heightfield colliders. Connects to the registry's signals:
    // construct it before the component initializers are connected, so its listeners run after them.
    PhysicsSystem(entt::registry& registry, JointMatrixFunction joint_matrix, std::function<bool()> is_editing, TerrainLookup terrain_lookup);
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    ~PhysicsSystem();

    // syncs the bodies to the transforms, applies the forces, steps the simulation and writes the dynamic bodies back
    // to their transforms; dt in milliseconds
    void update(float dt);

    // see engineCharacterControllerMove; dt in milliseconds
    engine_result_code_t move_character(entt::entity entity, const glm::vec3& velocity, float dt, engine_character_controller_move_result_t& out_result);
    // see enginePhysicsRayCast
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, const PhysicsWorld::QueryFilter& filter, engine_ray_hit_t& out_hit);
    // see enginePhysicsOverlapSphere: the game objects found, unsorted, into `out`
    void overlap_sphere(const glm::vec3& center, float radius, const PhysicsWorld::QueryFilter& filter, std::vector<OverlapHit>& out);
    // applied with the next update (a body created this frame isn't simulated yet); false without a body or an unknown type
    bool add_force(entt::entity entity, std::array<float, 3> force, engine_force_type_t type);
    void set_gravity(std::array<float, 3> gravity);
    glm::vec3 get_gravity() const;
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
    // creates the bodies of heightfield colliders again whose terrain's heights changed (or which appeared or went)
    void sync_heightfields_to_terrains();
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
    TerrainLookup terrain_lookup_;
    // TerrainData::get_change_count() when the heightfields were last compared with their terrains
    std::uint64_t terrain_changes_seen_ = 0;
    PhysicsWorld world_;
    // game objects whose transform changed since the last sync (a reactive storage: owned by the registry, so destroyed
    // game objects leave it; a standalone one would keep their stale entities)
    entt::storage_for_t<entt::reactive>& moved_bodies_;
    // reported moves of static bodies skip these: being placed in the frame they were created is no move
    std::vector<entt::entity> bodies_created_this_frame_;
    // a transform, parent or joint attachment changed since the bodies were last synced to the transforms
    bool transforms_changed_ = false;
    std::deque<std::tuple<entt::entity, std::array<float, 3>, engine_force_type_t>> forces_to_apply_;
};
}  // namespace engine
