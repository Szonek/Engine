#include "physics_world.h"
#include "math_helpers.h"
#include "logger.h"
#include "graphics.h"
#include "profiler.h"
#include "shaders_binding_slots.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <fmt/format.h>

#include <cassert>
#include <cmath>
#include <optional>

namespace
{
struct BodyPose
{
    btTransform transform;
    btVector3 scale;
};

// a world matrix as a Bullet transform (translation and rotation) and the scale of the collision shape
std::optional<BodyPose> to_body_pose(const glm::mat4& local_to_world)
{
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    // decompose leaves its outputs uninitialized when it fails; a zero scale divides by zero
    if (std::abs(glm::determinant(local_to_world)) < 1e-12f || !glm::decompose(local_to_world, scale, rotation, translation, skew, perspective))
    {
        return std::nullopt;
    }
    BodyPose pose;
    pose.transform.setIdentity();
    pose.transform.setOrigin(btVector3(translation.x, translation.y, translation.z));
    pose.transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
    pose.scale = btVector3(scale.x, scale.y, scale.z);
    return pose;
}
}  // namespace




engine::PhysicsWorld::PhysicsWorld()
    : debug_drawer_(std::make_unique<DebugDrawer>())
{
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

void engine::PhysicsWorld::collect_debug_lines(std::vector<DebugLineGpuData>& out_lines)
{
    if (is_debug_drawer_enabled())
    {
        dynamics_world_->debugDrawWorld();
        debug_drawer_->take_lines(out_lines);
    }
    else
    {
        engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Physics debug draw is not enabled!\n"));
    }
}

engine::physcic_internal_component_t engine::PhysicsWorld::create_rigid_body(const engine_collider_component_t& collider, BodyType type, float mass, const glm::mat4& local_to_world, std::int32_t body_index)
{
    physcic_internal_component_t ret{};
    if (type != BodyType::eDynamic)
    {
        mass = 0.0f;
    }
    // a world transform with a zero scale (e.g. a joint scaled to zero by an animation) can't place a body: it starts at
    // the origin and gets its transform and scale when it is moved to a valid one
    const auto pose = to_body_pose(local_to_world).value_or(BodyPose{ btTransform::getIdentity(), btVector3(1.0f, 1.0f, 1.0f) });
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

    ret.collision_shape->setLocalScaling(pose.scale);
    btVector3 local_inertia(0, 0, 0);
    if (mass > 0.0f)
    {
        ret.collision_shape->calculateLocalInertia(mass, local_inertia);
    }


    //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
    //ToDo: Validate if motion state should be used instead?
    btDefaultMotionState* my_motion_state = nullptr;// new btDefaultMotionState(transform_init);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, my_motion_state, ret.collision_shape, local_inertia);
    ret.rigid_body = new btRigidBody(rbInfo);
    ret.rigid_body->setFriction(collider.friction_static);
    ret.rigid_body->setRestitution(collider.bounciness);
    ret.rigid_body->setWorldTransform(pose.transform);
    if (collider.is_trigger)
    {
        // keeps the static flag the constructor sets for a body without mass
        ret.rigid_body->setCollisionFlags(ret.rigid_body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }
    ret.rigid_body->setUserIndex(body_index);
    if (type == BodyType::eKinematic)
    {
        ret.rigid_body->setCollisionFlags(ret.rigid_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        // Bullet computes a kinematic body's velocity from its moves, only while it is awake
        ret.rigid_body->setActivationState(DISABLE_DEACTIVATION);
    }
    add_to_world(ret.rigid_body);
    return ret;
}

engine::PhysicsWorld::BodyType engine::PhysicsWorld::get_body_type(const btRigidBody* rigid_body)
{
    if (rigid_body->isKinematicObject())
    {
        return BodyType::eKinematic;
    }
    return rigid_body->isStaticObject() ? BodyType::eStatic : BodyType::eDynamic;
}

void engine::PhysicsWorld::add_to_world(btRigidBody* rigid_body)
{
    if (rigid_body->isKinematicObject())
    {
        // Bullet's default puts kinematic bodies into the static group, which doesn't pair with itself: moving
        // triggers (hitboxes) wouldn't see a kinematic player then. Kinematic bodies pair with everything but statics.
        dynamics_world_->addRigidBody(rigid_body, btBroadphaseProxy::KinematicFilter, btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);
    }
    else
    {
        dynamics_world_->addRigidBody(rigid_body);
    }
}

bool engine::PhysicsWorld::set_world_transform(btRigidBody* rigid_body, const glm::mat4& local_to_world)
{
    const auto maybe_pose = to_body_pose(local_to_world);
    if (!maybe_pose)
    {
        return false;
    }
    const auto& pose = *maybe_pose;
    auto* shape = rigid_body->getCollisionShape();
    if (shape && shape->getLocalScaling() != pose.scale)
    {
        shape->setLocalScaling(pose.scale);
        if (rigid_body->getInvMass() > 0.0f)
        {
            const auto mass = 1.0f / rigid_body->getInvMass();
            btVector3 local_inertia(0.0f, 0.0f, 0.0f);
            shape->calculateLocalInertia(mass, local_inertia);
            rigid_body->setMassProps(mass, local_inertia);
            rigid_body->updateInertiaTensor();
        }
    }
    rigid_body->activate(true);
    rigid_body->setWorldTransform(pose.transform);
    if (rigid_body->getBroadphaseHandle())
    {
        dynamics_world_->updateSingleAabb(rigid_body);
    }
    return true;
}

void engine::PhysicsWorld::update(float dt)
{
    dynamics_world_->stepSimulation(dt, 10);

    collisions_desc_cache_.clear();
}

const std::vector<engine::CollisionDesc>& engine::PhysicsWorld::get_collisions() const
{
    // Collisions were computed this frame already, no need to do anything
    if (!collisions_desc_cache_.empty())
    {
        return collisions_desc_cache_;
    }
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

        CollisionDesc new_collision{};
        new_collision.contact_points.reserve(num_contacts);
        new_collision.object_a = manifold->getBody0()->getUserIndex();
        new_collision.object_b = manifold->getBody1()->getUserIndex();

        for (auto j = 0; j < num_contacts; j++)
        {
            const auto pt = manifold->getContactPoint(j);

            const auto& position_a = pt.getPositionWorldOnA();
            const auto& position_b = pt.getPositionWorldOnB();

            CollisionContactPointDesc new_contact_point{};
            new_contact_point.lifetime = pt.getLifeTime();

            new_contact_point.point_on_obj_a = { position_a.getX(), position_a.getY(), position_a.getZ() };
            new_contact_point.point_on_obj_b = { position_b.getX(), position_b.getY(), position_b.getZ() };
            new_collision.contact_points.push_back(new_contact_point);
            //log::log(log::LogLevel::eTrace, fmt::format("PT: {}\n", pt.getDistance()));
        }

        collisions_desc_cache_.push_back(new_collision);
    }

    return collisions_desc_cache_;
}

void engine::PhysicsWorld::set_gravity(std::span<const float> g)
{
    dynamics_world_->setGravity(btVector3(g[0], g[1], g[2]));
}

engine_ray_hit_info_t engine::PhysicsWorld::raycast(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance)
{
    struct RayWithIgnoreResultCallbackClosestHit : public btCollisionWorld::ClosestRayResultCallback
    {
        RayWithIgnoreResultCallbackClosestHit(const btVector3& rayFromWorld, const btVector3& rayToWorld, std::span<const engine_game_object_t>& ignore_list)
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

    struct RayWithIgnoreResultCallbackAllHits : public btCollisionWorld::AllHitsRayResultCallback
    {
        RayWithIgnoreResultCallbackAllHits(const btVector3& rayFromWorld, const btVector3& rayToWorld, std::span<const engine_game_object_t>& ignore_list)
            : btCollisionWorld::AllHitsRayResultCallback(rayFromWorld, rayToWorld)
            , ignore_list_(ignore_list)
        {
        }


        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
        {
            for (const auto& go : ignore_list_)
            {
                if (rayResult.m_collisionObject->getUserIndex() == go)
                    return 1.0;
            }

            return AllHitsRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }

    protected:
        std::span<const engine_game_object_t>& ignore_list_;
    };

    //RayWithIgnoreResultCallbackAllHits all_hits_result(
    //    btVector3(ray.origin[0], ray.origin[1], ray.origin[2]),
    //    btVector3(ray.direction[0], ray.direction[1], ray.direction[2]),
    //    ignore_list);
    //dynamics_world_->rayTest(
    //    btVector3(ray.origin[0], ray.origin[1], ray.origin[2]),
    //    btVector3(ray.direction[0], ray.direction[1], ray.direction[2]),
    //    all_hits_result
    //);

    RayWithIgnoreResultCallbackClosestHit closest_result(
        btVector3(ray.origin.x, ray.origin.y, ray.origin.z),
        btVector3(ray.direction.x, ray.direction.y, ray.direction.z),
        ignore_list);
    dynamics_world_->rayTest(
        btVector3(ray.origin.x, ray.origin.y, ray.origin.z),
        btVector3(ray.direction.x, ray.direction.y, ray.direction.z),
        closest_result
    );
    engine_ray_hit_info_t ret{};
    if (closest_result.hasHit())
    {
        ret.go = closest_result.m_collisionObject->getUserIndex();
        ret.position.x = closest_result.m_hitPointWorld.getX();
        ret.position.y = closest_result.m_hitPointWorld.getY();
        ret.position.z = closest_result.m_hitPointWorld.getZ();
        ret.normal.x = closest_result.m_hitNormalWorld.getX();
        ret.normal.y = closest_result.m_hitNormalWorld.getY();
        ret.normal.z = closest_result.m_hitNormalWorld.getZ();
    }
    return ret;
}

void engine::PhysicsWorld::DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
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

void engine::PhysicsWorld::DebugDrawer::take_lines(std::vector<DebugLineGpuData>& out_lines)
{
    //ToDo: Lifetime of lines is not taken into account. It should be removed after some time (lifeteam decreased with each update), but curretnly all lines are removed when they are taken.
    //ToDo: limit the renderable lines for only visisble? or use some kind of frustrum culling?
    out_lines.insert(out_lines.end(), lines_.begin(), lines_.end());
    lines_.clear();
}
