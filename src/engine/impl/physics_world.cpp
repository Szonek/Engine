#include "physics_world.h"
#include "math_helpers.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <cassert>


engine::PhysicsWorld::PhysicsWorld()
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
    //transform_init.setRotation()

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