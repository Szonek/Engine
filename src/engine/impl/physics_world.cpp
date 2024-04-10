#include "physics_world.h"
#include "math_helpers.h"
#include "logger.h"
#include "graphics.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <fmt/format.h>

#include <cassert>




engine::PhysicsWorld::PhysicsWorld()
    : debug_drawer_(nullptr)
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
    if (enable && debug_drawer_)
    {
        engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Physics debug draw is already enabled, you cant do enable it again!\n"));
        return;
    }
    if(enable)
    {
        debug_drawer_ = std::make_unique<DebugDrawer>();
        dynamics_world_->setDebugDrawer(debug_drawer_.get());
        debug_drawer_->setDebugMode(
            btIDebugDraw::DBG_DrawWireframe
            | btIDebugDraw::DBG_DrawAabb 
            | btIDebugDraw::DBG_DrawContactPoints
        );
    }
    else
    {
        debug_drawer_.reset();
    }
}

void engine::PhysicsWorld::debug_draw(RenderContext* renderer, std::span<const float> view, std::span<const float> projection)
{
    if (debug_drawer_)
    {
        // set state
        debug_drawer_->set_renderer(renderer);
        debug_drawer_->set_view(view);
        debug_drawer_->set_projection(projection);

        // draw
        dynamics_world_->debugDrawWorld();

        // reset state, so its not accidiently used for next frame
        // todo: possible optimization is not to reset state for each frame as camera is not changing
        debug_drawer_->set_renderer(nullptr);
        debug_drawer_->set_view({});
        debug_drawer_->set_projection({});
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

    const auto glm_pos = glm::make_vec3(transform.position);
    const auto glm_rot = glm::make_quat(transform.rotation);
    const auto glm_scl = glm::make_vec3(transform.scale);

    const auto model_matrix = compute_model_matrix(glm_pos, glm_rot, /*glm_scl*/ glm::vec3(1.0f));

    btTransform transform_init;
    transform_init.setFromOpenGLMatrix(glm::value_ptr(model_matrix));
    //transform_init.setIdentity();
    //transform_init.setOrigin(btVector3(transform.position[0], transform.position[1], transform.position[2]));
    //transform_init.setRotation(btQuaternion(transform.rotation[0], transform.rotation[1], transform.rotation[2], transform.rotation[3]));
    //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* my_motion_state = new btDefaultMotionState(transform_init);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(rigid_body.mass, my_motion_state, ret.collision_shape, local_inertia);
    ret.rigid_body = new btRigidBody(rbInfo);
    if (collider.is_trigger)
    {
        ret.rigid_body->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }
    //ret.rigid_body->setLinearFactor(btVector3(1.0f, 1.0f, 0.0f));
    //ret.rigid_body->setAngularFactor(btVector3(0.0f, 0.0f, 0.0f));
    ret.rigid_body->setUserIndex(body_index);

    //if (body_index == 3)
    //{
        //ret.rigid_body->setLinearFactor(btVector3(0.0, 0.0, 0.0));
        //ret.rigid_body->setAngularFactor(btVector3(0.0, 0.0, 0.0));
        //ret.rigid_body->setDamping(1.0f, 1.0f);
    //}


    //ret.rigid_body->setRestitution(collider.bounciness);
    //ret.rigid_body->setFriction(collider.friction_static);
    //ret.rigid_body->setLinearVelocity(btVector3(rigid_body.linear_velocity[0], rigid_body.linear_velocity[1], rigid_body.linear_velocity[2]));
    //ret.rigid_body->setCcdMotionThreshold(1e-7f);
    //ret.rigid_body->setCcdSweptSphereRadius(transform.scale[0]);
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
            const auto position_b = pt.getPositionWorldOnA();

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

void engine::PhysicsWorld::DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
    //ToDo: this can be optimized by using single geometry for all lines
    // i.e. store all lines in single buffer and draw them all at once
    // or use instanced rendering
    // batch lines to vertex and defer rendering till the end to reduce draw calls
    assert(renderer_ && "Renderer is not set in physics debug drawer!");
    const auto from_v = glm::vec3(from.getX(), from.getY(), from.getZ());
    const auto to_v = glm::vec3(to.getX(), to.getY(), to.getZ());
    const auto color_v = glm::vec3(color.getX(), color.getY(), color.getZ());

    std::array<glm::vec3, 2> vertices = { from_v, to_v };
    std::array<Geometry::vertex_attribute_t, 1> vertex_attributes = {
        { 0u, 3u, 0u, 0u, Geometry::vertex_attribute_t::Type::eFloat32 }
    };
    Geometry line_geo(vertex_attributes, { reinterpret_cast<const std::byte*>(vertices.data()), vertices.size() * sizeof(vertices[0])}, vertices.size());

    line_geo.bind();
    line_geo.draw(Geometry::Mode::eLines);
    //engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] draw line \n"));
}

void engine::PhysicsWorld::DebugDrawer::drawContactPoint(const btVector3& point_on_B, const btVector3& normal_on_B, btScalar distance, int life_time, const btVector3& color)
{
    //engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] draw contact point \n"));
}

void engine::PhysicsWorld::DebugDrawer::reportErrorWarning(const char* warning_string)
{
    engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] physics warning: {}\n", warning_string));
}

void engine::PhysicsWorld::DebugDrawer::draw3dText(const btVector3& location, const char* text_ttring)
{
    engine::log::log(engine::log::LogLevel::eTrace, fmt::format("[Bullet] draw 3d text {}\n", text_ttring));
}
