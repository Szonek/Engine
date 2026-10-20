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

    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider,
        const engine_rigid_body_component_t& rigid_body, const engine_transform_component_t& transform, std::int32_t body_index);

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

    // moves a body by hand (kinematic/static bodies, children of other game objects). Its broadphase bounds are
    // updated right away, so ray casts see it before the next simulation substep (a frame can have none).
    void set_world_transform(btRigidBody* rigid_body, const btTransform& world_transform);

    void set_gravity(std::span<const float> g);

    engine_ray_hit_info_t raycast(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance);

private:
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