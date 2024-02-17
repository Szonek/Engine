#include "scene.h"
#include "animation.h"
#include "ui_manager.h"
#include "logger.h"
#include "math_helpers.h"


#include <fmt/format.h>


#include <SDL3/SDL.h>

#include <RmlUi/Core.h>


engine::Scene::Scene(engine_result_code_t& out_code)
    : shader_simple_(Shader("simple.vs", "simple.fs"))
    , collider_create_observer(entity_registry_, entt::collector.group<engine_tranform_component_t, engine_collider_component_t>(entt::exclude<engine_rigid_body_component_t>))
    , transform_update_collider_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>().where<PhysicsWorld::physcic_internal_component_t>())
    , transform_model_matrix_update_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>())
    , rigid_body_create_observer(entity_registry_, entt::collector.group<engine_rigid_body_component_t, engine_tranform_component_t, engine_collider_component_t>())
    , rigid_body_update_observer(entity_registry_, entt::collector.update<engine_rigid_body_component_t>().where<engine_tranform_component_t, engine_collider_component_t>())
{
    entity_registry_.on_construct<engine_collider_component_t>().connect<&entt::registry::emplace<PhysicsWorld::physcic_internal_component_t>>();
    entity_registry_.on_destroy<PhysicsWorld::physcic_internal_component_t>().connect<&PhysicsWorld::remove_rigid_body>(&physics_world_);
    out_code = ENGINE_RESULT_CODE_OK;
}

engine::Scene::~Scene()
{  
}

engine_result_code_t engine::Scene::physics_update(float dt)
{
    // collider created, create internal rigid body
    for (const auto entt : collider_create_observer)
    {
        const auto collider_component = get_component<engine_collider_component_t>(entt);
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        auto physcics_component = get_component<PhysicsWorld::physcic_internal_component_t>(entt);

        // Create dummy rigid body component with mass 0.0f. 
        // Object is not dynamic. Such object cant be moved with velocity
        // Such object will be taking part in collisions.
        engine_rigid_body_component_t rigidbody_component{};
        rigidbody_component.mass = 0.0f;

        *physcics_component = physics_world_.create_rigid_body(*collider_component, rigidbody_component, *transform_component, static_cast<std::int32_t>(entt));
    }

    // detect new group creation, when rigid body component was added
    for (const auto entt : rigid_body_create_observer)
    {
        const auto collider_component = get_component<engine_collider_component_t>(entt);
        const auto rigidbody_component = get_component<engine_rigid_body_component_t>(entt);
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        auto physcics_component = get_component< PhysicsWorld::physcic_internal_component_t>(entt);
        *physcics_component = physics_world_.create_rigid_body(*collider_component, *rigidbody_component, *transform_component, static_cast<std::int32_t>(entt));
    }

    // transform component updated, sync it with rigid body
    for (const auto entt : transform_update_collider_observer)
    {
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        auto physcics_component = get_component<PhysicsWorld::physcic_internal_component_t>(entt);
        btTransform& world_transform = physcics_component->rigid_body->getWorldTransform();
        world_transform.setOrigin(btVector3(transform_component->position[0], transform_component->position[1], transform_component->position[2]));
        btQuaternion quaterninon{};
        quaterninon.setEulerZYX(transform_component->rotation[2], transform_component->rotation[1], transform_component->rotation[0]);
        world_transform.setRotation(quaterninon);

        physcics_component->rigid_body->setWorldTransform(world_transform);
    }

    // detect if rigid body component was updated by the user
    for (const auto entt : rigid_body_update_observer)
    {
        const auto rigidbody_component = get_component<engine_rigid_body_component_t>(entt);
        auto physcics_component = get_component<PhysicsWorld::physcic_internal_component_t>(entt);

        physcics_component->rigid_body->setLinearVelocity(btVector3(rigidbody_component->linear_velocity[0], rigidbody_component->linear_velocity[1], rigidbody_component->linear_velocity[2]));
        physcics_component->rigid_body->setAngularVelocity(btVector3(rigidbody_component->angular_velocity[0], rigidbody_component->angular_velocity[1], rigidbody_component->angular_velocity[2]));
    }

    physics_world_.update(dt / 1000.0f);
    //physics_world_.update(10.0f / 1000.0f);

    // sync physcis to graphics world
    // ToDo: this could be seperate function or called at the beggning of the graphics update function?
    auto transform_physcis_view = entity_registry_.view<engine_tranform_component_t, const PhysicsWorld::physcic_internal_component_t, engine_rigid_body_component_t>();
    transform_physcis_view.each([this](auto entity, engine_tranform_component_t transform, const PhysicsWorld::physcic_internal_component_t physcics, engine_rigid_body_component_t rigidbody)
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
            update_component(entity, transform);

            const auto lin_vel = physcics.rigid_body->getLinearVelocity();
            rigidbody.linear_velocity[0] = lin_vel.getX();
            rigidbody.linear_velocity[1] = lin_vel.getY();
            rigidbody.linear_velocity[2] = lin_vel.getZ();

            const auto ang_vel = physcics.rigid_body->getAngularVelocity();
            rigidbody.angular_velocity[0] = ang_vel.getX();
            rigidbody.angular_velocity[1] = ang_vel.getY();
            rigidbody.angular_velocity[2] = ang_vel.getZ();
            update_component(entity, rigidbody);
        }
    );

    collider_create_observer.clear();
    rigid_body_create_observer.clear();
    transform_update_collider_observer.clear();
    rigid_body_update_observer.clear();

    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engine::Scene::update(RenderContext& rdx, float dt, std::span<const Texture2D> textures, 
    std::span<const Geometry> geometries, std::span<const AnimationClipData> animations)
{
    // transform component updated, calculate new model matrix
    for (const auto entt : transform_model_matrix_update_observer)
    {
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        const auto glm_pos = glm::make_vec3(transform_component->position);
        const auto glm_rot = glm::make_vec3(transform_component->rotation);
        const auto glm_scl = glm::make_vec3(transform_component->scale);

        // if animaion is playing than we dont need to do below local_to_world math here
        // it will be recomputed in animation loop
        // so for such cases it's performance lose
        const auto model_matrix = compute_model_matrix(glm_pos, glm_rot, glm_scl);
        std::memcpy(transform_component->local_to_world, &model_matrix, sizeof(model_matrix));
    }

    auto animation_view = entity_registry_.view<engine_tranform_component_t, engine_animation_component_t>();

    for (auto [entity, transform, animation] : animation_view.each())
    {
        for (auto i = 0; i < ENGINE_ANIMATIONS_CLIPS_MAX_COUNT; i++)
        {
            if (animation.animations_state[i] == ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING)
            {
                continue;
            }

            const auto& animation_data = animations[animation.animations_array[i]];
            auto& animation_dt = animation.animations_dt[i];
            animation_dt += dt;

            auto matrix = animation_data.compute_animation_model_matrix(glm::make_mat4(transform.local_to_world), animation_dt);
            std::memcpy(transform.local_to_world, &matrix, sizeof(matrix));

            
            if (animation_dt >= animation_data.get_duration())
            {
                animation.animations_state[i] = ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING;
                animation_dt = 0.0f;
            }
        }
    }

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

        //rdx.set_polygon_mode(RenderContext::PolygonFaceType::eFrontAndBack, RenderContext::PolygonMode::eLine);
        geometry_renderet.each([this, &rdx, &textures, &geometries](const engine_tranform_component_t& transform, const engine_mesh_component_t& mesh, const engine_material_component_t& material)
            {
                if (mesh.disable)
                {
                    return;
                }

                shader_simple_.bind();
                shader_simple_.set_uniform_f4("diffuse_color", material.diffuse_color);
                shader_simple_.set_uniform_mat_f4("model", transform.local_to_world);

                const auto texture_diffuse_idx = material.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material.diffuse_texture;
                shader_simple_.set_texture("texture_diffuse", &textures[texture_diffuse_idx]);

                geometries[mesh.geometry].bind();
                geometries[mesh.geometry].draw(Geometry::Mode::eTriangles);

			}
		);
        //rdx.set_polygon_mode(RenderContext::PolygonFaceType::eFrontAndBack, RenderContext::PolygonMode::eFill);
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

entt::runtime_view engine::Scene::create_runtime_view()
{
    return entt::runtime_view{};
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

