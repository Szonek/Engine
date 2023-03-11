#include "scene.h"
#include "text_2d_manager.h"

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

    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider, const engine_rigid_body_component_t& rigid_body, const engine_tranform_component_t& transform)
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

        dynamics_world_->addRigidBody(ret.rigid_body);

        return ret;
    }

    void update(float dt)
    {
        dynamics_world_->stepSimulation(dt, 10);
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
};

inline physics_world_t physics_world_;



engine::Scene::Scene(engine_result_code_t& out_code)
    : shader_simple_(Shader("simple.vs", "simple.fs"))
{
    entity_registry_.on_construct<engine_rigid_body_component_t>().connect<&entt::registry::emplace<physics_world_t::physcic_internal_component_t>>();
    out_code = ENGINE_RESULT_CODE_OK;
}

engine::Scene::~Scene()
{
}

engine_result_code_t engine::Scene::physics_update(float dt)
{

    auto physcis_view = entity_registry_.view<physics_world_t::physcic_internal_component_t, engine_tranform_component_t, const engine_rigid_body_component_t, const engine_collider_component_t>();
    physcis_view.each([](physics_world_t::physcic_internal_component_t& physics, engine_tranform_component_t& transform, const engine_rigid_body_component_t rigidbody, const engine_collider_component_t collider)
        {
            // requires update or init
            if (!physics.rigid_body)
            {
                physics = physics_world_.create_rigid_body(collider, rigidbody, transform);
            }

        }
    );
    //physics_world_.update(dt);
    physics_world_.update(1.f / 120.f);
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engine::Scene::update(RenderContext& rdx, float dt, std::span<const Texture2D> textures, std::span<const Geometry> geometries, TextManager* text_mgn)
{
    // TRANSFORM SYSTEM
    auto transform_view = entity_registry_.view<engine_tranform_component_t>(entt::exclude<engine_rigid_body_component_t>);
    transform_view.each([](engine_tranform_component_t& transform)
        {
            const auto glm_pos = glm::make_vec3(transform.position);
            const auto glm_rot = glm::make_vec3(transform.rotation);
            const auto glm_scl = glm::make_vec3(transform.scale);

            const auto model_matrix = compute_model_matirx(glm_pos, glm_rot, glm_scl);
            std::memcpy(transform.local_to_world, &model_matrix, sizeof(model_matrix));
        }
    );

    auto transform_physcis_view = entity_registry_.view<engine_tranform_component_t, const physics_world_t::physcic_internal_component_t>();
    transform_physcis_view.each([](engine_tranform_component_t& transform, const physics_world_t::physcic_internal_component_t physcics)
        {
            btTransform transform_phsycics{};
            physcics.rigid_body->getMotionState()->getWorldTransform(transform_phsycics);   

            glm::mat4 model_matrix;
            transform_phsycics.getOpenGLMatrix(glm::value_ptr(model_matrix));

            const auto glm_scl = glm::make_vec3(transform.scale);
            model_matrix = glm::scale(model_matrix, glm_scl);
            std::memcpy(transform.local_to_world, &model_matrix, sizeof(model_matrix));
        }
    );

	auto geometry_renderet = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t>();
	auto text_renderer = entity_registry_.view<const engine_rect_tranform_component_t, const engine_material_component_t, const engine_text_component_t>();
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
                shader_simple_.bind();
                shader_simple_.set_uniform_f4("diffuse_color", material.diffuse_color);
                shader_simple_.set_uniform_mat_f4("model", transform.local_to_world);

                geometries[mesh.geometry].bind();
                geometries[mesh.geometry].draw(Geometry::Mode::eTriangles);
			}
		);

        text_renderer.each([this, &rdx, &window_size_pixels, &text_mgn](const engine_rect_tranform_component_t& transform, const engine_material_component_t& material, const engine_text_component_t& text)
            {

                const auto glm_pos = glm::vec3(transform.position[0] * window_size_pixels.width, transform.position[1] * window_size_pixels.height, 0.0f);
                const auto glm_rot = glm::vec3(0.0f, 0.0f, 0.0f);
                const auto glm_scl = glm::vec3(transform.scale[0], transform.scale[1], 1.0f);
                const auto model_matrix = compute_model_matirx(glm_pos, glm_rot, glm_scl);

                text_mgn->render_text(rdx, text.text, text.font_handle, { glm::value_ptr(model_matrix), sizeof(model_matrix) / sizeof(float) }, window_size_pixels.width, window_size_pixels.height);
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

