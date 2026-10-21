#pragma once
#include "engine.h"
#include "collision_desc.h"

#ifdef _MSC_VER
#pragma warning(disable: 4127) // disable warning
#endif
#include <btBulletDynamicsCommon.h>
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
    // collision shape. The mass is only used by dynamic bodies.
    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider, BodyType type, float mass,
        const glm::mat4& local_to_world, std::int32_t body_index);
    static BodyType get_body_type(const btRigidBody* rigid_body);
    // A character controller's body: a kinematic capsule standing on the origin of local_to_world, scaled by its scale.
    physcic_internal_component_t create_character_body(float radius, float height, const glm::mat4& local_to_world, std::int32_t body_index);

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
    }

    void update(float dt);
    const std::vector<CollisionDesc>& get_collisions() const;

    // moves a body by hand to a world transform (kinematic bodies, dynamic ones moved by their game object), rescaling its
    // shape when the scale changed. Its broadphase bounds are updated right away, so ray casts see it before the next
    // simulation substep (a frame can have none). False (body unchanged) when the matrix can't be decomposed.
    bool set_world_transform(btRigidBody* rigid_body, const glm::mat4& local_to_world);

    void set_gravity(std::span<const float> g);

    engine_ray_hit_info_t raycast(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance);

private:
    // adds the body to the world with the broadphase filter of its type
    void add_to_world(btRigidBody* rigid_body);

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
    std::unique_ptr<btBroadphaseInterface> overlapping_pair_cache_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;

    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world_;

    mutable std::vector<CollisionDesc> collisions_desc_cache_;
};

}// namespace engine