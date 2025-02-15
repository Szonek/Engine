#include "physics_world.h"
#include "math_helpers.h"
#include "logger.h"
#include "graphics.h"
#include "profiler.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <fmt/format.h>

#include <cassert>




engine::PhysicsWorld::PhysicsWorld(RenderContext* renderer)
    : debug_drawer_(std::make_unique<DebugDrawer>(renderer))
{

    collisions_info_buffer_.reserve(1024 * 2);
    collisions_contact_points_buffer_.reserve(1024 * 16);

    collision_config_ = std::make_unique<btDefaultCollisionConfiguration>();
    dispatcher_ = std::make_unique<btCollisionDispatcher>(collision_config_.get());
    overlapping_pair_cache_ = std::make_unique<btDbvtBroadphase>();
    solver_ = std::make_unique<btSequentialImpulseConstraintSolver>();

    dynamics_world_ = std::make_unique<btDiscreteDynamicsWorld>(dispatcher_.get(), overlapping_pair_cache_.get(), solver_.get(), collision_config_.get());

    set_gravity(std::array<float, 3>{ 0.0f, -10.0f, 0.0f });


    //keep track of the shapes, we release memory at exit.
    //make sure to re-use collision shapes among rigid bodies whenever possible!
    //btAlignedObjectArray<btCollisionShape*> collisionShapes;
}

void engine::PhysicsWorld::enable_debug_draw(bool enable)
{
    if (enable && is_debug_drawer_enabled())
    {
        return; // nothing to do
    }
    if(enable)
    {
        dynamics_world_->setDebugDrawer(debug_drawer_.get());
        debug_drawer_->setDebugMode(
            btIDebugDraw::DBG_DrawWireframe
            | btIDebugDraw::DBG_DrawAabb 
            | btIDebugDraw::DBG_DrawContactPoints
        );
    }
    else
    {
        dynamics_world_->setDebugDrawer(nullptr);
    }
}

bool engine::PhysicsWorld::is_debug_drawer_enabled() const
{
    return dynamics_world_->getDebugDrawer() != nullptr;
}

void engine::PhysicsWorld::debug_draw(const glm::mat4& view, const glm::mat4& projection)
{
    if (is_debug_drawer_enabled())
    {
        debug_drawer_->begin_frame(view, projection);
        dynamics_world_->debugDrawWorld();
        debug_drawer_->end_frame();
    }
    else
    {
        engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Physics debug draw is not enabled!\n"));
    }
}

engine::PhysicsWorld::physcic_internal_component_t engine::PhysicsWorld::create_rigid_body(const engine_collider_component_t& collider, const engine_rigid_body_component_t& rigid_body, const engine_tranform_component_t& transform, std::int32_t body_index)
{
    physcic_internal_component_t ret{};
    if (collider.type == ENGINE_COLLIDER_TYPE_BOX)
    {
        const btVector3 box_bounds{
            collider.collider.box.size[0],
            collider.collider.box.size[1],
            collider.collider.box.size[2],
        };
        ret.collision_shape = new btBoxShape(box_bounds);
    }
    else if (collider.type == ENGINE_COLLIDER_TYPE_SPHERE)
    {
        ret.collision_shape = new btSphereShape(collider.collider.sphere.radius);
    }
    else if (collider.type == ENGINE_COLLIDER_TYPE_COMPOUND)
    {
        auto cs = new btCompoundShape();
        for (auto i = 0; i < ENGINE_COMPOUND_COLLIDER_MAX_CHILD_COLLIDERS; i++)
        {
            const auto& child_collider = collider.collider.compound.children[i];
            
            auto shape_transform = btTransform();
            shape_transform.setIdentity();
            shape_transform.setOrigin(btVector3(child_collider.transform[0], child_collider.transform[1], child_collider.transform[2]));
            shape_transform.setRotation(btQuaternion(child_collider.rotation_quaternion[0], child_collider.rotation_quaternion[1], child_collider.rotation_quaternion[2], child_collider.rotation_quaternion[3]));

            if (child_collider.type == ENGINE_COLLIDER_TYPE_BOX)
            {
                cs->addChildShape(shape_transform, new btBoxShape(btVector3(child_collider.collider.box.size[0], child_collider.collider.box.size[1], child_collider.collider.box.size[2])));
            }
            else if (child_collider.type == ENGINE_COLLIDER_TYPE_SPHERE)
            {
                cs->addChildShape(shape_transform, new btSphereShape(child_collider.collider.sphere.radius));
            }
            else
            {
                engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Unknown collider type in compound collider!\n"));
                return ret;
            }
        }
        ret.collision_shape = cs;
    }
    else
    {
        assert(false && "Unknown collider type in physisc world!");
        return ret;
    }

    ret.collision_shape->setLocalScaling(btVector3(transform.scale[0], transform.scale[1], transform.scale[2]));
    btVector3 local_inertia(0, 0, 0);
    if (rigid_body.mass)
    {
        ret.collision_shape->calculateLocalInertia(rigid_body.mass, local_inertia);
    }

    btTransform transform_init;
    transform_init.setIdentity();
    transform_init.setOrigin(btVector3(transform.position[0], transform.position[1], transform.position[2]));
    transform_init.setRotation(btQuaternion(transform.rotation[0], transform.rotation[1], transform.rotation[2], transform.rotation[3]));

    //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
    //ToDo: Validate if motion state should be used instead?
    btDefaultMotionState* my_motion_state = nullptr;// new btDefaultMotionState(transform_init);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(rigid_body.mass, my_motion_state, ret.collision_shape, local_inertia);
    ret.rigid_body = new btRigidBody(rbInfo);
    ret.rigid_body->setWorldTransform(transform_init);
    if (collider.is_trigger)
    {
        ret.rigid_body->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }
    //dynamics_world_->getCollisionWorld().
    //if (rigid_body.mass == 0.0f)
    //{
    //    ret.rigid_body->setCollisionFlags(ret.rigid_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    //    ret.rigid_body->setActivationState(DISABLE_DEACTIVATION);
    //}

    ret.rigid_body->setUserIndex(body_index);
    dynamics_world_->addRigidBody(ret.rigid_body);

    return ret;
}

void engine::PhysicsWorld::update(float dt)
{
    dynamics_world_->stepSimulation(dt, 10);
}

const std::vector<engine_collision_info_t>& engine::PhysicsWorld::get_collisions()
{
    collisions_info_buffer_.clear();
    collisions_contact_points_buffer_.clear();

    const auto num_manifolds = dynamics_world_->getDispatcher()->getNumManifolds();
    //log::log(log::LogLevel::eTrace, fmt::format("Collisions: {}\n", num_manifolds));

    for (auto i = 0; i < num_manifolds; i++)
    {
        const auto manifold = dynamics_world_->getDispatcher()->getManifoldByIndexInternal(i);

        const auto num_contacts = manifold->getNumContacts();
        //log::log(log::LogLevel::eTrace, fmt::format("Num contacts: {}\n", num_contacts));
        if (num_contacts == 0)
        {
            continue;
        }


        engine_collision_info_t new_collision{};
        new_collision.contact_points_count = num_contacts;
        new_collision.contact_points = collisions_contact_points_buffer_.data() + collisions_contact_points_buffer_.size();
        new_collision.object_a = static_cast<engine_game_object_t>(manifold->getBody0()->getUserIndex());
        new_collision.object_b = static_cast<engine_game_object_t>(manifold->getBody1()->getUserIndex());

        for (auto j = 0; j < num_contacts; j++)
        {
            const auto pt = manifold->getContactPoint(j);

            const auto position_a = pt.getPositionWorldOnA();
            const auto position_b = pt.getPositionWorldOnB();

            engine_collision_contact_point_t new_contact_point{};
            new_contact_point.lifetime = pt.getLifeTime();

            new_contact_point.point_object_a[0] = position_a.getX();
            new_contact_point.point_object_a[1] = position_a.getY();
            new_contact_point.point_object_a[2] = position_a.getZ();

            new_contact_point.point_object_b[0] = position_b.getX();
            new_contact_point.point_object_b[1] = position_b.getY();
            new_contact_point.point_object_b[2] = position_b.getZ();

            collisions_contact_points_buffer_.push_back(new_contact_point);
            //log::log(log::LogLevel::eTrace, fmt::format("PT: {}\n", pt.getDistance()));
        }

        collisions_info_buffer_.push_back(new_collision);
    }

    return collisions_info_buffer_;
}

void engine::PhysicsWorld::set_gravity(std::span<const float> g)
{
    dynamics_world_->setGravity(btVector3(g[0], g[1], g[2]));
}

engine_ray_hit_info_t engine::PhysicsWorld::raycast(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance)
{
    struct RayWithIgnoreResultCallback : public btCollisionWorld::ClosestRayResultCallback
    {
        RayWithIgnoreResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld, std::span<const engine_game_object_t>& ignore_list)
            : btCollisionWorld::ClosestRayResultCallback(rayFromWorld, rayToWorld)
            , ignore_list_(ignore_list)
        {
        }


        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
        {
            for(const auto& go : ignore_list_)
            {
                if (rayResult.m_collisionObject->getUserIndex() == go)
                    return 1.0;
            }

            return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }

    protected:
        std::span<const engine_game_object_t>& ignore_list_;
    };

    RayWithIgnoreResultCallback closest_result(
        btVector3(ray.origin[0], ray.origin[1], ray.origin[2]),
        btVector3(ray.direction[0], ray.direction[1], ray.direction[2]),
        ignore_list);
    dynamics_world_->rayTest(
        btVector3(ray.origin[0], ray.origin[1], ray.origin[2]),
        btVector3(ray.direction[0], ray.direction[1], ray.direction[2]),
        closest_result
    );
    engine_ray_hit_info_t ret{};
    if (closest_result.hasHit())
    {
        ret.go = closest_result.m_collisionObject->getUserIndex();
        ret.position[0] = closest_result.m_hitPointWorld.getX();
        ret.position[1] = closest_result.m_hitPointWorld.getY();
        ret.position[2] = closest_result.m_hitPointWorld.getZ();
        ret.normal[0] = closest_result.m_hitNormalWorld.getX();
        ret.normal[1] = closest_result.m_hitNormalWorld.getY();
        ret.normal[2] = closest_result.m_hitNormalWorld.getZ();
    }
    return ret;
}

engine::PhysicsWorld::DebugDrawer::DebugDrawer(RenderContext* renderer)
    : renderer_(renderer)
{
    const auto projected_max_lines_count = 2048;  // ToDo: make it configurable? 
    const auto ssbo_size = sizeof(LineDrawPacket) * projected_max_lines_count;
    if (renderer_->get_limits().ssbo_max_size < ssbo_size)
    {
        ssbo_ = ShaderStorageBuffer(ssbo_size);
    }
    lines_.reserve(projected_max_lines_count);
}

void engine::PhysicsWorld::DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
    assert(renderer_ && "Renderer is not set in physics debug drawer!");
    const auto from_v = glm::vec3(from.getX(), from.getY(), from.getZ());
    const auto to_v = glm::vec3(to.getX(), to.getY(), to.getZ());
    const auto color_v = glm::vec3(color.getX(), color.getY(), color.getZ());

    lines_.push_back({ from_v, 0.0f, to_v, 0.0f, color_v, 1 });
    //engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] draw line \n"));
}

void engine::PhysicsWorld::DebugDrawer::drawContactPoint(const btVector3& point_on_B, const btVector3& normal_on_B, btScalar distance, int life_time, const btVector3& color)
{
    // Calculate the start and end points of the line segment
    btVector3 from = point_on_B;
    btVector3 to = point_on_B + normal_on_B * distance * 10;

    // Convert the Bullet vectors to GLM vectors
    glm::vec3 from_v(from.getX(), from.getY(), from.getZ());
    glm::vec3 to_v(to.getX(), to.getY(), to.getZ());
    glm::vec3 color_v(color.getX(), color.getY(), color.getZ());

    //lines_.push_back({ from_v, to_v, color_v, life_time });
    //engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] draw contact point \n"));
}

void engine::PhysicsWorld::DebugDrawer::reportErrorWarning(const char* warning_string)
{
    engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] physics warning: {}\n", warning_string));
}

void engine::PhysicsWorld::DebugDrawer::draw3dText(const btVector3& /*location*/, const char* text_ttring)
{
    engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] draw 3d text {}\n", text_ttring));
}

void engine::PhysicsWorld::DebugDrawer::begin_frame(const glm::mat4& view, const glm::mat4& projection)
{
    set_view(view);
    set_projection(projection);
}

void engine::PhysicsWorld::DebugDrawer::end_frame()
{
    process_lines_buffer();
    set_view({});
    set_projection({});
}

void engine::PhysicsWorld::DebugDrawer::process_lines_buffer()
{
    ENGINE_PROFILE_SECTION_N("physics_process_lines_buffer");
    if (lines_.empty())
    {
        return;
    }


    //ToDo: Lifetime of lines is not taken into account. It should be removed after some time (lifeteam decreased with each update), but curretnly all lines are removed at the end of this function.
    //ToDo: limit the renderable lines for only visisble? or use some kind of frustrum culling?

    static Shader shader_ssbo({ "debug_physics_lines_ssbo.vs" }, { "debug_physics_lines_ssbo.fs" });
    static Geometry line_geo_simple(2);

    {
        const auto lines_count = lines_.size();
        const auto ssbo_limit = ssbo_.get_size() / sizeof(LineDrawPacket);

        if (lines_count > ssbo_limit)
        {
            engine::log::log(engine::log::LogLevel::eTrace, fmt::format("Resizeing SSBO for line drawer in physics visual debugger. Limit is: {}. Current lines count: {}\n", ssbo_limit, lines_count));
            ssbo_ = ShaderStorageBuffer(sizeof(LineDrawPacket) * lines_count);
        }
    }


    BufferMapContext<LineDrawPacket, ShaderStorageBuffer> mapping_context(ssbo_, false, true);
    std::memcpy(mapping_context.data, lines_.data(), lines_.size() * sizeof(LineDrawPacket));
    mapping_context.unmap();
        
    shader_ssbo.bind();
    shader_ssbo.set_uniform_mat_f4("view", { glm::value_ptr(view_), sizeof(view_) / sizeof(float) });
    shader_ssbo.set_uniform_mat_f4("projection", { glm::value_ptr(projection_), sizeof(projection_) / sizeof(float) });

    ssbo_.bind(2);
    line_geo_simple.bind();
    line_geo_simple.draw_instances(Geometry::Mode::eLines, lines_.size());

    lines_.clear();

}
