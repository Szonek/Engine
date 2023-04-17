#include "scene.h"
#include "ui_manager.h"
#include "logger.h"

#include <fmt/format.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <SDL3/SDL.h>

#include <btBulletDynamicsCommon.h>

namespace
{
inline glm::mat4 compute_model_matirx(const glm::vec3& glm_pos, const glm::vec3& glm_rot)
{
    auto model_identity = glm::mat4{ 1.0f };
    auto translation = glm::translate(model_identity, glm_pos);
    translation *= glm::toMat4(glm::quat(glm_rot));
    return translation;
}
inline glm::mat4 compute_model_matirx(const glm::vec3& glm_pos, const glm::vec3& glm_rot, const glm::vec3& glm_scl)
{
    auto model_identity = glm::mat4{ 1.0f };
    auto translation = glm::translate(model_identity, glm_pos);
    translation *= glm::toMat4(glm::quat(glm_rot));
    //translation = glm::rotate(translation, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
    translation = glm::scale(translation, glm_scl);
    return translation;
}
}

namespace engine 
{

struct physics_world_t
{
public:
    struct physcic_internal_component_t
    {
        btCollisionShape* collision_shape = nullptr;
        btRigidBody* rigid_body = nullptr;
    };
public:
    physics_world_t()
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

    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider, const engine_rigid_body_component_t& rigid_body, const engine_tranform_component_t& transform, std::int32_t body_index)
    {
        physcic_internal_component_t ret{};
        if (collider.type == ENGINE_COLLIDER_TYPE_BOX)
        {
            const btVector3 box_bounds{
                collider.collider.box.size[0] * transform.scale[0] * 0.5f,
                collider.collider.box.size[1] * transform.scale[1] * 0.5f,
                collider.collider.box.size[2] * transform.scale[2] * 0.5f,
            };
            ret.collision_shape = new btBoxShape(box_bounds);     
        } 
        else if (collider.type == ENGINE_COLLIDER_TYPE_SPHERE)
        {
            ret.collision_shape = new btSphereShape(collider.collider.sphere.radius * transform.scale[0]);
        }
        else
        {
            assert(false && "Unknown collider type in physisc world!");
            return ret;
        }

        btVector3 local_inertia(0, 0, 0);
        if (rigid_body.mass)
        {
            ret.collision_shape->calculateLocalInertia(rigid_body.mass, local_inertia);
        }

        const auto glm_pos = glm::make_vec3(transform.position);
        const auto glm_rot = glm::make_vec3(transform.rotation);

        const auto model_matrix = compute_model_matirx(glm_pos, glm_rot);

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
        ret.rigid_body->setUserIndex(body_index);
        //ret.rigid_body->setCcdMotionThreshold(1e-7f);
        //ret.rigid_body->setCcdSweptSphereRadius(transform.scale[0]);
        dynamics_world_->addRigidBody(ret.rigid_body);

        return ret;
    }

    void update(float dt)
    {
        dynamics_world_->stepSimulation(dt, 5);
    }

    const std::vector<engine_collision_info_t>& get_collisions()
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

    void set_gravity(std::span<const float> g)
    {
        dynamics_world_->setGravity(btVector3(g[0], g[1], g[2]));

    }

    ~physics_world_t()
    {

    }

private:
    std::unique_ptr<btDefaultCollisionConfiguration> collision_config_;
    std::unique_ptr<btCollisionDispatcher> dispatcher_;
    std::unique_ptr<btBroadphaseInterface> overlapping_pair_cache_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;

    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world_;

    std::vector<engine_collision_info_t> collisions_info_buffer_;
    std::vector<engine_collision_contact_point_t> collisions_contact_points_buffer_;
};

} // namespace engine
inline engine::physics_world_t physics_world_;



engine::Scene::Scene(engine_result_code_t& out_code)
    : shader_simple_(Shader("simple.vs", "simple.fs"))
{
    entity_registry_.on_construct<engine_collider_component_t>().connect<&entt::registry::emplace<physics_world_t::physcic_internal_component_t>>();
    out_code = ENGINE_RESULT_CODE_OK;
}

engine::Scene::~Scene()
{
}

engine_result_code_t engine::Scene::physics_update(float dt)
{
    // view of objects which user created with rigid body component
    // such objects are dynamic, can be moved with velocity
    auto physcis_rigidbody_view = entity_registry_.view<physics_world_t::physcic_internal_component_t, engine_tranform_component_t, engine_rigid_body_component_t, const engine_collider_component_t>();
    physcis_rigidbody_view.each([](auto entity, physics_world_t::physcic_internal_component_t& physics, engine_tranform_component_t& transform, engine_rigid_body_component_t rigidbody, const engine_collider_component_t collider)
        {
            // requires update or init
            if (!physics.rigid_body)
            {
                physics = physics_world_.create_rigid_body(collider, rigidbody, transform, static_cast<std::int32_t>(entity));
            }

            btTransform world_transform;
            physics.rigid_body->getMotionState()->getWorldTransform(world_transform);
            auto& origin = world_transform.getOrigin();
            if (origin.getX() != transform.position[0] || origin.getY() != transform.position[1] || origin.getZ() != transform.position[2])
            {
                origin.setX(transform.position[0]);
                origin.setY(transform.position[1]);
                origin.setZ(transform.position[2]);
                physics.rigid_body->setWorldTransform(world_transform);
            }
            if (physics.rigid_body->getLinearVelocity().getX() != rigidbody.linear_velocity[0]
                || physics.rigid_body->getLinearVelocity().getY() != rigidbody.linear_velocity[1]
                || physics.rigid_body->getLinearVelocity().getZ() != rigidbody.linear_velocity[2])
            {
                physics.rigid_body->setLinearVelocity(btVector3(rigidbody.linear_velocity[0], rigidbody.linear_velocity[1], rigidbody.linear_velocity[2]));
            }
            if (physics.rigid_body->getAngularVelocity().getX() != rigidbody.angular_velocity[0]
                || physics.rigid_body->getAngularVelocity().getY() != rigidbody.angular_velocity[1]
                || physics.rigid_body->getAngularVelocity().getZ() != rigidbody.angular_velocity[2])
            {
                physics.rigid_body->setAngularVelocity(btVector3(rigidbody.angular_velocity[0], rigidbody.angular_velocity[1], rigidbody.angular_velocity[2]));
            }
        }
    );

    // view of colliders
    // objects which are taking part into collisions, but cant be moved with velocity
    auto physcis_colliders_view = entity_registry_.view<physics_world_t::physcic_internal_component_t, engine_tranform_component_t, const engine_collider_component_t>(entt::exclude<engine_rigid_body_component_t>);
    physcis_colliders_view.each([](auto entity, physics_world_t::physcic_internal_component_t& physics, engine_tranform_component_t& transform, const engine_collider_component_t collider)
        {
            // requires update or init
            if (!physics.rigid_body)
            {
                // Create dummy rigid body component with mass 0.0f. 
                // Object is not dynamic. Such object cant be moved with velocity
                // Such object will be taking part in collisions.
                engine_rigid_body_component_t rigidbody{};
                rigidbody.mass = 0.0f;
                physics = physics_world_.create_rigid_body(collider, rigidbody, transform, static_cast<std::int32_t>(entity));
            }
            
            btTransform& world_transform = physics.rigid_body->getWorldTransform();
            world_transform.setOrigin(btVector3(transform.position[0], transform.position[1], transform.position[2]));
            btQuaternion quaterninon{};
            quaterninon.setEulerZYX(transform.rotation[2], transform.rotation[1], transform.rotation[0]);
            world_transform.setRotation(quaterninon);
        }
    );

    physics_world_.update(dt / 1000.0f);
    
    // sync physcis to graphics world
    // ToDo: this could be seperate function or called at the beggning of the graphics update function?
    auto transform_physcis_view = entity_registry_.view<engine_tranform_component_t, const physics_world_t::physcic_internal_component_t, const engine_rigid_body_component_t>();
    transform_physcis_view.each([](engine_tranform_component_t& transform, const physics_world_t::physcic_internal_component_t physcics, const engine_rigid_body_component_t rigidbody)
        {
            assert(physcics.rigid_body);
            btTransform transform_phsycics{};
            physcics.rigid_body->getMotionState()->getWorldTransform(transform_phsycics);   

            const auto origin = transform_phsycics.getOrigin();
            transform.position[0] = origin.getX();
            transform.position[1] = origin.getY();
            transform.position[2] = origin.getZ();

            const auto euler_rotation = transform_phsycics.getRotation();
            euler_rotation.getEulerZYX(transform.rotation[2], transform.rotation[1], transform.rotation[0]);
        }
    );

    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engine::Scene::update(RenderContext& rdx, float dt, std::span<const Texture2D> textures, std::span<const Geometry> geometries, UiManager* ui_manager)
{
    // TRANSFORM SYSTEM
    auto transform_view = entity_registry_.view<engine_tranform_component_t>(/*entt::exclude<engine_rigid_body_component_t>*/);
    transform_view.each([](engine_tranform_component_t& transform)
        {
            const auto glm_pos = glm::make_vec3(transform.position);
            const auto glm_rot = glm::make_vec3(transform.rotation);
            const auto glm_scl = glm::make_vec3(transform.scale);

            const auto model_matrix = compute_model_matirx(glm_pos, glm_rot, glm_scl);
            std::memcpy(transform.local_to_world, &model_matrix, sizeof(model_matrix));
        }
    );

	auto geometry_renderet = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t>();
	auto ui_text_renderer = entity_registry_.view<const engine_rect_tranform_component_t , const engine_text_component_t>();
	auto ui_image_renderer = entity_registry_.view<const engine_rect_tranform_component_t , const engine_image_component_t>();
    auto camera_view = entity_registry_.view<const engine_camera_component_t, const engine_tranform_component_t>();


    for (auto [entity, camera, transform] : camera_view.each()) 
    {
        if (!camera.enabled)
        {
            continue;
        }

        const auto window_size_pixels = rdx.get_window_size_in_pixels();

        // update camera: view and projection
        {
            const auto z_near = camera.clip_plane_near;
            const auto z_far = camera.clip_plane_far;
            // ToD: multi camera - this should use resolution of camera!!!

            const auto adjusted_width = window_size_pixels.width * (camera.viewport_rect.width - camera.viewport_rect.x);
            const auto adjusted_height = window_size_pixels.height * (camera.viewport_rect.height - camera.viewport_rect.y);
            const float aspect = adjusted_width / adjusted_height;
            glm::mat4 projection;

            if (camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC)
            {
                const float scale = camera.type_union.orthographics_scale;
                projection = glm::ortho(-aspect * scale, aspect * scale, -scale, scale, z_near, z_far);
            }
            else
            {
                projection = glm::perspective(glm::radians(camera.type_union.perspective_fov), aspect, z_near, z_far);
            }
            const auto eye_position = glm::make_vec3(transform.position);
            const auto up = glm::make_vec3(camera.direction.up);
            const auto target = glm::make_vec3(camera.target);
            const auto view = glm::lookAt(eye_position, target, up);

            shader_simple_.bind();
            shader_simple_.set_uniform_mat_f4("view", { glm::value_ptr(view), sizeof(view) / sizeof(float) });
            shader_simple_.set_uniform_mat_f4("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });
        }

        geometry_renderet.each([this, &textures, &geometries](const engine_tranform_component_t& transform, const engine_mesh_component_t& mesh, const engine_material_component_t& material)
            {
                if (mesh.disable)
                {
                    return;
                }
                shader_simple_.bind();
                shader_simple_.set_uniform_f4("diffuse_color", material.diffuse_color);
                shader_simple_.set_uniform_mat_f4("model", transform.local_to_world);

                geometries[mesh.geometry].bind();
                geometries[mesh.geometry].draw(Geometry::Mode::eTriangles);
			}
		);

        ui_text_renderer.each([this, &rdx, &window_size_pixels, &ui_manager](const engine_rect_tranform_component_t& transform, const engine_text_component_t& text)
            {
                const auto glm_pos = glm::vec3(transform.position[0] * window_size_pixels.width, transform.position[1] * window_size_pixels.height, 0.0f);
                const auto glm_rot = glm::vec3(0.0f, 0.0f, 0.0f);
                const auto glm_scl = glm::vec3(transform.scale[0], transform.scale[1], 1.0f);
                const auto model_matrix = compute_model_matirx(glm_pos, glm_rot, glm_scl);

                ui_manager->render_text(rdx, text.text, text.font_handle, { glm::value_ptr(model_matrix), sizeof(model_matrix) / sizeof(float) });
            }
        );

        ui_image_renderer.each([this, &rdx, &window_size_pixels, &ui_manager](const engine_rect_tranform_component_t& transform, const engine_image_component_t& img)
           {
               const auto glm_pos = glm::vec3(transform.position[0] * window_size_pixels.width, transform.position[1] * window_size_pixels.height, 0.0f);
               const auto glm_rot = glm::vec3(0.0f, 0.0f, 0.0f);
               const auto glm_scl = glm::vec3(transform.scale[0] * window_size_pixels.width, transform.scale[1] * window_size_pixels.height, 1.0f);
               const auto model_matrix = compute_model_matirx(glm_pos, glm_rot, glm_scl);

               ui_manager->render_image(rdx, { glm::value_ptr(model_matrix), sizeof(model_matrix) / sizeof(float) });
           }
        );
    }

    return ENGINE_RESULT_CODE_OK;
}

entt::entity engine::Scene::create_new_entity()
{
    return entity_registry_.create();
}

void engine::Scene::destroy_entity(entt::entity entity)
{
    entity_registry_.destroy(entity);
}

void engine::Scene::set_physcis_gravity(std::array<float, 3> g)
{
    physics_world_.set_gravity(g);
}

void engine::Scene::get_physcis_collisions_list(const engine_collision_info_t*& ptr_first, size_t* count)
{
    assert(count != nullptr);
    const auto& collisions = physics_world_.get_collisions();
    ptr_first = collisions.data();
    *count = collisions.size();
}

