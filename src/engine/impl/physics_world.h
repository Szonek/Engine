#pragma once
#include "engine.h"
#include "collision_desc.h"
#include "overlap_results.h"
#include "collision_layers.h"
#include "terrain_data.h"

#ifdef _MSC_VER
#pragma warning(disable: 4127) // disable warning
#endif
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#ifdef _MSC_VER
#pragma warning(default: 4127) // enable warning back
#endif
#include "graphics.h"
#include "components_internals/physics_component.h"
#include "render/render_scene_data.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <array>
#include <span>

namespace engine
{
class PhysicsWorld
{
public:
    PhysicsWorld();

    /**
     * @brief Enables or disables debug drawing for the physics world.
     *
     * If enabled, collect_debug_lines will provide wireframes, axis-aligned bounding boxes (AABBs)
     * and contact points. If disabled, collect_debug_lines must not be called.
     *
     * @param enable A boolean value indicating whether to enable (true) or disable (false) debug drawing.
     */
    void enable_debug_draw(bool enable);
    bool is_debug_drawer_enabled() const;
    /**
     * @brief Appends lines visualizing the physics world to out_lines.
     *
     * Nothing is drawn here, lines are meant to be passed to the Renderer (see SceneRenderData::debug_lines).
     * Debug drawing has to be enabled with enable_debug_draw.
     */
    void collect_debug_lines(std::vector<DebugLineGpuData>& out_lines);

    enum class BodyType
    {
        eStatic,
        eKinematic,
        eDynamic,
    };
    // A body at the world transform of its game object: translation and rotation place the body, the scale scales its
    // collision shape. The mass is only used by dynamic bodies. `heightfield` is the terrain of a heightfield collider:
    // the shape reads its heights in place (the body holds them), and without it such a collider has no body.
    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider, BodyType type, float mass,
        const glm::mat4& local_to_world, std::int32_t body_index, std::shared_ptr<const TerrainData> heightfield = nullptr);
    static BodyType get_body_type(const btRigidBody* rigid_body);
    // A character controller's body: a kinematic capsule standing on the origin of local_to_world, scaled by its scale,
    // on `layer`, colliding with `collides_with` (collision_layers.h).
    physcic_internal_component_t create_character_body(float radius, float height, std::uint32_t layer, std::uint32_t collides_with,
        const glm::mat4& local_to_world, std::int32_t body_index);

    struct CharacterMoveResult
    {
        glm::vec3 position;  // of the feet, in world space
        bool grounded = false;
        // stands on something, walkable or not (an edge, a too steep slope)
        bool blocked_below = false;
        bool hit_wall = false;
        bool hit_ceiling = false;
    };
    // Sweeps the capsule of a character body from `feet` by `motion` (world space): steps up, slides along what blocks it,
    // steps down and snaps to walkable ground within step_height. Static and kinematic bodies block (not triggers);
    // dynamic bodies don't, the kinematic body pushes them. Moving out of an overlap is never blocked.
    CharacterMoveResult move_character(const btRigidBody* body, const glm::vec3& feet, const glm::vec3& motion, float step_height, float max_slope_degrees) const;
    glm::vec3 get_gravity() const;

    void remove_rigid_body(entt::basic_registry<entt::entity>& reg, entt::entity entt)
    {
        auto& comp = reg.get<physcic_internal_component_t>(entt);
        // bodies are created on the next scene update, so a game object destroyed before it has none yet
        if (!comp.rigid_body)
        {
            return;
        }
        dynamics_world_->removeRigidBody(comp.rigid_body);

        delete comp.rigid_body;
        comp.rigid_body = nullptr;
        delete_shape(comp.collision_shape);
        comp.collision_shape = nullptr;
        comp.shape_data.reset();
    }

    void update(float dt);
    const std::vector<CollisionDesc>& get_collisions() const;

    // moves a body by hand to a world transform (kinematic bodies, dynamic ones moved by their game object), rescaling its
    // shape when the scale changed. Its broadphase bounds are updated right away, so ray casts see it before the next
    // simulation substep (a frame can have none). False (body unchanged) when the matrix can't be decomposed.
    bool set_world_transform(btRigidBody* rigid_body, const glm::mat4& local_to_world);

    void set_gravity(std::span<const float> g);

    // what a query skips (engine_physics_query_filter_t): the ignored game objects, triggers unless hit_triggers, and
    // bodies on layers not in layer_mask (0: every layer)
    struct QueryFilter
    {
        std::span<const engine_game_object_t> ignore;
        bool hit_triggers = false;
        std::uint32_t layer_mask = 0;
    };
    // the closest hit along origin + direction * t, t in [0, max_distance] (direction need not be normalized), passing
    // the filter. False without a hit or for an empty ray.
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, const QueryFilter& filter, engine_ray_hit_t& out_hit);
    // The game objects whose bodies overlap `shape` at `pose` (a query shape, never added to the world), each with the
    // distance from the shape's surface to theirs at the deepest contact (negative: penetration), unsorted, into `out`
    void overlap(const btCollisionShape& shape, const btTransform& pose, const QueryFilter& filter, std::vector<OverlapHit>& out) const;
    // the game objects overlapping the sphere, with the distance from its center to their surface (negative inside), into `out`
    void overlap_sphere(const glm::vec3& center, float radius, const QueryFilter& filter, std::vector<OverlapHit>& out) const;

private:
    // a body's shape, with the shapes of its children when it is a compound
    static void delete_shape(btCollisionShape* shape);

    static constexpr float FIXED_TIME_STEP = 1.0f / 60.0f;
    static constexpr int MAX_SUB_STEPS = 10;

    // adds the body to the world on its layer: its broadphase group is the layer's bit, its mask the layers it collides
    // with (the pair filter below decides every pair)
    void add_to_world(btRigidBody* rigid_body, std::uint32_t layer, std::uint32_t collides_with);

    // The one place deciding which bodies interact: their layers (each in the other's mask), then the fixed body motion
    // rule (no static-static, no kinematic-static pairs), collision_layers.h.
    class PairFilter : public btOverlapFilterCallback
    {
    public:
        bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
    };

    class DebugDrawer : public btIDebugDraw
    {
    public:
        void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
        void drawContactPoint(const btVector3& point_on_B, const btVector3& normal_on_B, btScalar distance, int life_time, const btVector3& color) override;
        void reportErrorWarning(const char* warning_string) override;
        void draw3dText(const btVector3& location, const char* text_string) override;

        void setDebugMode(int debug_mode) override
        {
            debug_mode_ = debug_mode;
        }

        int getDebugMode() const override
        {
            return debug_mode_;
        }

        // moves lines gathered since the last call to out_lines
        void take_lines(std::vector<DebugLineGpuData>& out_lines);

    private:
        std::int32_t debug_mode_ = 0;
        std::vector<DebugLineGpuData> lines_;
    };

private:
    std::unique_ptr<DebugDrawer> debug_drawer_;
    std::unique_ptr<btDefaultCollisionConfiguration> collision_config_;
    std::unique_ptr<btCollisionDispatcher> dispatcher_;
    // before the broadphase, which keeps a pointer to it
    PairFilter pair_filter_;
    std::unique_ptr<btBroadphaseInterface> overlapping_pair_cache_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;

    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world_;

    mutable std::vector<CollisionDesc> collisions_desc_cache_;
};

}// namespace engine