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
#include <glm/glm.hpp>

#include "graphics.h"

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
    PhysicsWorld(class RenderContext* renderer);

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
    bool is_debug_drawer_enabled() const;
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
    void debug_draw(const glm::mat4& view, const glm::mat4& projection);

    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider,
        const engine_rigid_body_component_t& rigid_body, const engine_tranform_component_t& transform, std::int32_t body_index);

    void remove_rigid_body(entt::basic_registry<entt::entity>& reg, entt::entity entt)
    {
        auto& comp = reg.get<physcic_internal_component_t>(entt);
        dynamics_world_->removeRigidBody(comp.rigid_body);

        delete comp.rigid_body;
        comp.rigid_body = nullptr;
    }

    void update(float dt);

    const std::vector<engine_collision_info_t>& get_collisions();

    void set_gravity(std::span<const float> g);

    engine_ray_hit_info_t raycast(const engine_ray_t& ray, float max_distance);

private:
    class DebugDrawer : public btIDebugDraw
    {
    public:
        DebugDrawer(RenderContext* renderer);
        void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
        void drawContactPoint(const btVector3& point_on_B, const btVector3& normal_on_B, btScalar distance, int life_time, const btVector3& color) override;
        void reportErrorWarning(const char* warning_string) override;
        void draw3dText(const btVector3& location, const char* text_string) override;

        void begin_frame(const glm::mat4& view, const glm::mat4& projection);

        void end_frame();

        void setDebugMode(int debug_mode) override
        {
            debug_mode_ = debug_mode;
        }

        int getDebugMode() const override
        {
            return debug_mode_;
        }

    private:
        void set_view(const glm::mat4& view)
        {
            view_ = view;
        }

        void set_projection(const glm::mat4& projection)
        {
            projection_ = projection;
        }

        void process_lines_buffer();

    private:
        struct LineDrawPacket
        {
            glm::vec3 from;
            float pad0;
            glm::vec3 to;
            float pad1;
            glm::vec3 color;
            float pad2;
        };
        struct DrawableLine {
            glm::vec3 from;
            glm::vec3 to;
            glm::vec3 color;
            std::int32_t life_time = 0;
        };
    private:
        RenderContext* renderer_ = nullptr;
        ShaderStorageBuffer ssbo_;
        glm::mat4 view_;
        glm::mat4 projection_;
        std::int32_t debug_mode_;

        std::vector<DrawableLine> lines_;
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