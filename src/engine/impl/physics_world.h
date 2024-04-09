#pragma once
#include "engine.h"

#ifdef _MSC_VER
#pragma warning(disable: 4127) // disable warning
#endif
#include <btBulletDynamicsCommon.h>
#ifdef _MSC_VER
#pragma warning(default: 4127) // enable warning back
#endif


#include <vector>
#include <memory>
#include <array>
#include <span>

#include <entt/entt.hpp>

namespace engine
{

class PhysicsWorld
{
public:
    struct physcic_internal_component_t
    {
        btCollisionShape* collision_shape = nullptr;
        btRigidBody* rigid_body = nullptr;

    };
public:
    PhysicsWorld();

    /**
     * @brief Enables or disables debug drawing for the physics world.
     *
     * This function is used to control whether debug information for the physics world is drawn.
     * If enabled, the debug_draw function will draw wireframes, axis-aligned bounding boxes (AABBs),
     * and contact points. If disabled, the debug_draw function will do nothing.
     *
     * @param enable A boolean value indicating whether to enable (true) or disable (false) debug drawing.
     *
     * @note Debug drawing requires that you have a valid OpenGL context and that your
     *       OpenGL state is correctly set up for rendering lines and points.
     */
    void enable_debug_draw(bool enable);

    /**
     * @brief Draws the debug information for the physics world.
     *
     * This function is used to visually debug the physics world. It will draw wireframes,
     * axis-aligned bounding boxes (AABBs), and contact points if debug drawing has been enabled
     * using the enable_debug_draw function. If debug drawing is not enabled, this function
     * will do nothing.
     *
     * This function should be called in your render loop, after all your other rendering but
     * before you swap buffers and poll events.
     *
     * @note This function requires that you have a valid OpenGL context and that your
     *       OpenGL state is correctly set up for rendering lines and points.
     */
    void debug_draw(class RenderContext* renderer, std::span<const float> view, std::span<const float> projection);

    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider,
        const engine_rigid_body_component_t& rigid_body, const engine_tranform_component_t& transform, std::int32_t body_index);

    void remove_rigid_body(entt::basic_registry<entt::entity>& reg, entt::entity entt)
    {
        const auto comp = reg.get<physcic_internal_component_t>(entt);
        dynamics_world_->removeRigidBody(comp.rigid_body);
    }

    void update(float dt);

    const std::vector<engine_collision_info_t>& get_collisions();

    void set_gravity(std::span<const float> g);

private:
    class DebugDrawer : public btIDebugDraw
    {
    public:

        void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
        void drawContactPoint(const btVector3& point_on_B, const btVector3& normal_on_B, btScalar distance, int life_time, const btVector3& color) override;
        void reportErrorWarning(const char* warning_string) override;
        void draw3dText(const btVector3& location, const char* text_string) override;
        
        // has to be called each frame
        void set_renderer(class RenderContext* renderer)
        {
            renderer_ = renderer;
        }

        void set_view(std::span<const float> view)
        {
            view_ = view.data();
        }

        void set_projection(std::span<const float> projection)
        {
            projection_ = projection.data();
        }

        void setDebugMode(int debug_mode) override
        {
            debug_mode_ = debug_mode;
        }

        int getDebugMode() const override
        {
            return debug_mode_;
        }

    private:
        class RenderContext* renderer_ = nullptr;
        const float* view_ = nullptr;
        const float* projection_ = nullptr;
        std::int32_t debug_mode_;
    };

private:
    std::unique_ptr<DebugDrawer> debug_drawer_;
    std::unique_ptr<btDefaultCollisionConfiguration> collision_config_;
    std::unique_ptr<btCollisionDispatcher> dispatcher_;
    std::unique_ptr<btBroadphaseInterface> overlapping_pair_cache_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;

    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world_;

    std::vector<engine_collision_info_t> collisions_info_buffer_;
    std::vector<engine_collision_contact_point_t> collisions_contact_points_buffer_;
};

}// namespace engine