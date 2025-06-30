#include "application.h"
#include "scene.h"
#include "ui_manager.h"
#include "nav_mesh.h"
#include "logger.h"
#include "math_helpers.h"
#include "components_utils/components_initializers.h"
#include "components_internals/camera_internal_component.h"
#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"
#include "profiler.h"
#include "engine_vector_impl_def.h"
#include "skin.h"

#include <fmt/format.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <SDL3/SDL.h>

#include <RmlUi/Core.h>

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"

#include "imgui/imgui.h"
#include "imguizmo/ImGuizmo.h"

#include <cmath>

struct SceneGpuData
{
    std::uint32_t direction_light_count = 0;
    std::uint32_t point_light_count = 0;
    std::uint32_t spot_light_count = 0;
    float pad3_;
};

struct LightGpuData
{
    glm::vec3 position;
    float cutoff;
    glm::vec3 direction;
    float outer_cutoff;
    float constant;
    float linear;
    float quadratic;
    float pad0_;
    glm::vec3 ambient;
    float pad1_;
    glm::vec3 diffuse;
    float pad2_;
    glm::vec3 specular;
    float pad3_;
};

static inline void update_parent_component(entt::registry& registry, entt::entity entity)
{
    auto& parent = registry.get<engine_parent_component_t>(entity);
    if (parent.parent == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Parent component has invalid parent id. If entity doesnt have parent than just delete it. Are you sure you are doing valid thing?\n"));
        return;
    }
    const auto parent_entt = static_cast<entt::entity>(parent.parent);
    engine_children_component_t* cc = registry.try_get<engine_children_component_t>(parent_entt);
    if (!cc)
    {
        cc = &registry.emplace<engine_children_component_t>(parent_entt);
    }
    for (auto i = 0; i < ENGINE_MAX_CHILDREN; i++)
    {
        if (cc->child[i] == ENGINE_INVALID_GAME_OBJECT_ID)
        {
            cc->child[i] = static_cast<std::uint32_t>(entity);
            return;
        }
              
    }
    engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Parent component has no more space for children. Are you sure you are doing valid thing?\n"));
}

static inline void destroy_parent_component(entt::registry& registry, entt::entity entity)
{
    const auto parent_entt = static_cast<entt::entity>(registry.get<engine_parent_component_t>(entity).parent);
    auto& cc = registry.get<engine_children_component_t>(parent_entt);
    for (auto i = 0; i < ENGINE_MAX_CHILDREN; i++)
    {
        if (cc.child[i] == static_cast<std::uint32_t>(entity))
        {
            cc.child[i] = ENGINE_INVALID_GAME_OBJECT_ID;
            return;
        }
    }
    engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Parent component was destroyed, but couldn't reset it's childer.\n"));
}
static inline void calculate_camera_view_and_projection(std::size_t window_width, std::size_t window_height, const glm::vec3& eye_position, const engine_camera_component_t& camera, engine::camera_internal_component_t& camera_internal)
{
    // update camera: view and projection
    ENGINE_PROFILE_SECTION_N("camera_update");
    if(!camera_internal.computed_this_frame)
    {
        camera_internal.computed_this_frame = true;
        const auto z_near = camera.clip_plane_near;
        const auto z_far = camera.clip_plane_far;
        // ToD: multi camera - this should use resolution of camera!!!

        const auto adjusted_width = window_width * (camera.viewport_rect.width - camera.viewport_rect.x);
        const auto adjusted_height = window_height * (camera.viewport_rect.height - camera.viewport_rect.y);
        const float aspect = adjusted_width / adjusted_height;

        if (camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC)
        {
            const float scale = camera.type_union.orthographics_scale;
            camera_internal.data.projection = glm::ortho(-aspect * scale, aspect * scale, -scale, scale, z_near, z_far);
        }
        else
        {
            camera_internal.data.projection = glm::perspective(glm::radians(camera.type_union.perspective_fov), aspect, z_near, z_far);
        }
        const auto up = glm::make_vec3(camera.direction.up);
        const auto target = glm::make_vec3(camera.target);
        camera_internal.data.view = glm::lookAt(eye_position, target, up);
    }
}

engine::Scene::Scene(Application* app, RenderContext& rdx, const engine_scene_create_desc_t& config, engine_result_code_t& out_code)
    : app_(app)
    , rdx_(rdx)
    , physics_world_(&rdx_)
    //, fbo_draw_scene_(rdx.get_window_size_in_pixels().width, rdx.get_window_size_in_pixels().height, 1, true)
    //, fbo_draw_outline_(rdx.get_window_size_in_pixels().width, rdx.get_window_size_in_pixels().height, 1, false)
    //, empty_vao_for_full_screen_quad_draw_(6)
    , collider_create_observer(entity_registry_, entt::collector.group<engine_tranform_component_t, engine_collider_component_t>(entt::exclude<engine_rigid_body_component_t>))
    , collider_update_observer(entity_registry_, entt::collector.update<engine_collider_component_t>().where<engine_tranform_component_t>())
    , transform_update_collider_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>().where<physcic_internal_component_t>())
    , transform_model_matrix_update_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>())
    , mesh_update_observer(entity_registry_, entt::collector.update<engine_mesh_component_t>())
    , rigid_body_create_observer(entity_registry_, entt::collector.group<engine_rigid_body_component_t, engine_tranform_component_t, engine_collider_component_t>())
    , rigid_body_update_observer(entity_registry_, entt::collector.update<engine_rigid_body_component_t>().where<engine_tranform_component_t, engine_collider_component_t>())
    , scene_ubo_(sizeof(SceneGpuData))
    , light_data_ssbo_(1'000 * sizeof(LightGpuData))
    //, full_screen_shader_(Shader({ "full_screen_quad.vs" }, { "full_screen_quad.fs" }))
{
    // basic initalizers
    entity_registry_.on_construct<engine_tranform_component_t>().connect<&initialize_transform_component>();
    entity_registry_.on_construct<engine_mesh_component_t>().connect<&initialize_mesh_component>();
    entity_registry_.on_construct<engine_material_component_t>().connect<&initialize_material_component>();
    entity_registry_.on_construct<engine_parent_component_t>().connect<&initialize_parent_component>();
    entity_registry_.on_construct<engine_name_component_t>().connect<&initialize_name_component>();
    entity_registry_.on_construct<engine_camera_component_t>().connect<&initialize_camera_component>();
    entity_registry_.on_construct<engine_camera_component_t>().connect<&entt::registry::emplace<camera_internal_component_t>>();
    entity_registry_.on_construct<engine_rigid_body_component_t>().connect<&initialize_rigidbody_component>();
    entity_registry_.on_construct<engine_collider_component_t>().connect<&initialize_collider_component>();
    entity_registry_.on_construct<engine_skin_component_t>().connect<&initialize_skin_component>();
    entity_registry_.on_construct<engine_light_component_t>().connect<&initialize_light_component>();
    entity_registry_.on_construct<engine_sprite_component_t>().connect<&initialize_sprite_component>();
    
    entity_registry_.on_update<engine_parent_component_t>().connect<&update_parent_component>();
    entity_registry_.on_destroy<engine_parent_component_t>().connect<&destroy_parent_component>();

    entity_registry_.on_construct<engine_collider_component_t>().connect<&entt::registry::emplace<physcic_internal_component_t>>();
    entity_registry_.on_destroy<engine_collider_component_t>().connect<&entt::registry::remove<physcic_internal_component_t>>();
    entity_registry_.on_destroy<physcic_internal_component_t>().connect<&PhysicsWorld::remove_rigid_body>(&physics_world_);
    out_code = ENGINE_RESULT_CODE_OK;

    
}

engine::Scene::~Scene()
{  
}

engine::Application* engine::Scene::get_application() const
{
    return app_;
}

void engine::Scene::enable_physics_debug_draw(bool enable)
{
    physics_world_.enable_debug_draw(enable);
}

engine_result_code_t engine::Scene::physics_update(float dt)
{
    ENGINE_PROFILE_SECTION_N("physics_update");
    // collider created, create internal rigid body
    for (const auto entt : collider_create_observer)
    {
        const auto collider_component = get_component<engine_collider_component_t>(entt);
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        auto physcics_component = *get_component<physcic_internal_component_t>(entt);
        if (physcics_component.rigid_body)
        {
            physics_world_.remove_rigid_body(entity_registry_, entt);
        }

        // Create dummy rigid body component with mass 0.0f. 
        // Object is not dynamic. Such object cant be moved with velocity
        // Such object will be taking part in collisions.
        engine_rigid_body_component_t rigidbody_component{};
        rigidbody_component.mass = 0.0f;

        physcics_component = physics_world_.create_rigid_body(*collider_component, rigidbody_component, *transform_component, static_cast<std::int32_t>(entt));
        update_component(entt, physcics_component);
    }
    collider_create_observer.clear();

    // detect new group creation, when rigid body component was added
    for (const auto entt : rigid_body_create_observer)
    {
        const auto collider_component = get_component<engine_collider_component_t>(entt);
        const auto rigidbody_component = get_component<engine_rigid_body_component_t>(entt);
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        auto physcics_component = *get_component<physcic_internal_component_t>(entt);
        if (physcics_component.rigid_body)
        {
            physics_world_.remove_rigid_body(entity_registry_, entt);
        }
        physcics_component = physics_world_.create_rigid_body(*collider_component, *rigidbody_component, *transform_component, static_cast<std::int32_t>(entt));
        update_component(entt, physcics_component);
    }
    rigid_body_create_observer.clear();
    
    // in cases when collider was updated, remove old rigid body and create new one to have physics world updated
    // this can happen in run-time (saldom), or through editor (more often)
    for (const auto entt : collider_update_observer)
    {
        physics_world_.remove_rigid_body(entity_registry_, entt);

        const auto collider_component = get_component<engine_collider_component_t>(entt);
        const auto transform_component = get_component<engine_tranform_component_t>(entt);
        auto physcics_component = *get_component<physcic_internal_component_t>(entt);

        engine_rigid_body_component_t rigidbody_component{};
        if (has_component<engine_rigid_body_component_t>(entt))
        {
            rigidbody_component = *get_component<engine_rigid_body_component_t>(entt);
        }
        else
        {
            rigidbody_component.mass = 0.0f;
        }
        physcics_component = physics_world_.create_rigid_body(*collider_component, rigidbody_component, *transform_component, static_cast<std::int32_t>(entt));
        update_component(entt, physcics_component);
    }
    collider_update_observer.clear();

    // transform component updated, sync it with rigid body
    // as a rule of thumb: if rigid body has mass (is dynamic) than it cant be moved by transform component
    for (const auto entity : transform_update_collider_observer)
    //auto transform_physcis_preupdate = entity_registry_.view<const engine_tranform_component_t, PhysicsWorld::physcic_internal_component_t>();
    //transform_physcis_preupdate.each([this](auto entity, const engine_tranform_component_t& transform_component, PhysicsWorld::physcic_internal_component_t& physcics_component)
        {
            const auto transform_component = *get_component<engine_tranform_component_t>(entity);
            auto& physcics_component = *get_component<physcic_internal_component_t>(entity);
            btTransform world_transform;
            //physcics_component->rigid_body->getMotionState()->getWorldTransform(world_transform);
            world_transform = physcics_component.rigid_body->getWorldTransform();

            if (has_component<engine_parent_component_t>(entity))
            {
                //btTransform& world_transform = physcics_component.rigid_body->getWorldTransform();
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(glm::make_mat4(transform_component.local_to_world), scale, rotation, translation, skew, perspective);
                world_transform.setOrigin(btVector3(translation.x, translation.y, translation.z));
                const btQuaternion quaterninon(rotation.x, rotation.y, rotation.z, rotation.w);
                world_transform.setRotation(quaterninon);
            }
            else
            {
                world_transform.setOrigin(btVector3(transform_component.position[0], transform_component.position[1], transform_component.position[2]));
                const btQuaternion quaterninon(transform_component.rotation[0], transform_component.rotation[1], transform_component.rotation[2], transform_component.rotation[3]);
                world_transform.setRotation(quaterninon);
            }

            //physcics_component->rigid_body->translate(btVector3(translation.x, translation.y, translation.z));
            physcics_component.rigid_body->activate(true);
            physcics_component.rigid_body->setWorldTransform(world_transform);
        }
    //);
    transform_update_collider_observer.clear();

    // detect if rigid body component was updated by the user
    //for (const auto entt : rigid_body_update_observer)
    //{
    //    const auto rigidbody_component = get_component<engine_rigid_body_component_t>(entt);
    //    auto physcics_component = get_component<PhysicsWorld::physcic_internal_component_t>(entt);

    //    physcics_component->rigid_body->activate(true);
    //    physcics_component->rigid_body->setLinearVelocity(btVector3(rigidbody_component->linear_velocity[0], rigidbody_component->linear_velocity[1], rigidbody_component->linear_velocity[2]));
    //    physcics_component->rigid_body->setAngularVelocity(btVector3(rigidbody_component->angular_velocity[0], rigidbody_component->angular_velocity[1], rigidbody_component->angular_velocity[2]));
    //}
    //rigid_body_update_observer.clear();

    for (const auto& f : forces_to_apply_)
    {
        const auto entity = std::get<0>(f);
        const auto force = std::get<1>(f);
        const auto type = std::get<2>(f);
        if (!has_component<physcic_internal_component_t>(entity))
        {
            continue;
        }
        auto& physcics = *get_component<physcic_internal_component_t>(entity);
        if (!physcics.rigid_body)
        {
            continue;
        }
        if (type == engine_force_type_t::ENGINE_FORCE_TYPE_FORCE)
        {
            physcics.rigid_body->applyCentralForce(btVector3(force[0], force[1], force[2]));
        }
        else if (type == engine_force_type_t::ENGINE_FORCE_TYPE_IMPLUSE)
        {
            physcics.rigid_body->applyCentralImpulse(btVector3(force[0], force[1], force[2]));
        }
    }
    forces_to_apply_.clear();

    physics_world_.update(dt / 1000.0f);

    // sync physcis to graphics world
    // ToDo: this could be seperate function or called at the beggning of the graphics update function?
    auto transform_physcis_view_post_update = entity_registry_.view<engine_tranform_component_t, const physcic_internal_component_t, engine_rigid_body_component_t>();
    transform_physcis_view_post_update.each([this](auto entity, engine_tranform_component_t& transform, const physcic_internal_component_t& physcics, engine_rigid_body_component_t& rigidbody)
        {
            //assert(physcics.rigid_body);
            if (!physcics.rigid_body)
            {
                return;
            }
            btTransform transform_phsycics{};
            transform_phsycics = physcics.rigid_body->getWorldTransform();
            //physcics.rigid_body->getMotionState()->getWorldTransform(transform_phsycics);   

            const auto& origin = transform_phsycics.getOrigin();
            transform.position[0] = origin.getX();
            transform.position[1] = origin.getY();
            transform.position[2] = origin.getZ();

            const auto& euler_rotation = transform_phsycics.getRotation();
            transform.rotation[0] = euler_rotation.getX();
            transform.rotation[1] = euler_rotation.getY();
            transform.rotation[2] = euler_rotation.getZ();
            transform.rotation[3] = euler_rotation.getW();
            update_component(entity, transform);

            //glm::mat4 mat;
            //transform_phsycics.getOpenGLMatrix(glm::value_ptr(mat));
            //mat = glm::scale(mat, glm::make_vec3(transform.scale));
            //std::memcpy(transform.local_to_world, &mat, sizeof(mat));
            //update_component(entity, transform);

            //const auto lin_vel = physcics.rigid_body->getLinearVelocity();
            //rigidbody.linear_velocity[0] = lin_vel.getX();
            //rigidbody.linear_velocity[1] = lin_vel.getY();
            //rigidbody.linear_velocity[2] = lin_vel.getZ();

            //const auto ang_vel = physcics.rigid_body->getAngularVelocity();
            //rigidbody.angular_velocity[0] = ang_vel.getX();
            //rigidbody.angular_velocity[1] = ang_vel.getY();
            //rigidbody.angular_velocity[2] = ang_vel.getZ();
            //update_component(entity, rigidbody);
        }
    );
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engine::Scene::update(float dt)
{
    ENGINE_PROFILE_SECTION_N("scene_update");
    physics_update(dt);

    {
        ENGINE_PROFILE_SECTION_N("transform_view");
#if 1
        //auto transform_view = entity_registry_.view<engine_tranform_component_t>(entt::exclude<engine_rigid_body_component_t>);
        auto transform_view = entity_registry_.view<engine_tranform_component_t>();
        transform_view.each([this](engine_tranform_component_t& transform_component)
            {
                //ZoneScopedN("scene_transform_view");
                const auto glm_pos = glm::make_vec3(transform_component.position);
                const auto glm_rot = glm::make_quat(transform_component.rotation);
                const auto glm_scl = glm::make_vec3(transform_component.scale);

                auto model_matrix = compute_model_matrix(glm_pos, glm_rot, glm_scl);
                std::memcpy(transform_component.local_to_world, &model_matrix, sizeof(model_matrix));
            });
#else
        // transform component updated, calculate new model matrix
        for (const auto entt : transform_model_matrix_update_observer)
        {
            const auto transform_component = get_component<engine_tranform_component_t>(entt);
            const auto glm_pos = glm::make_vec3(transform_component->position);
            const auto glm_rot = glm::make_quat(transform_component->rotation);
            const auto glm_scl = glm::make_vec3(transform_component->scale);

            auto model_matrix = compute_model_matrix(glm_pos, glm_rot, glm_scl);
            std::memcpy(transform_component->local_to_world, &model_matrix, sizeof(model_matrix));
        }
        transform_model_matrix_update_observer.clear();
#endif

    }

    {
        ENGINE_PROFILE_SECTION_N("skin_update_view");
        auto skin_updated_view = entity_registry_.view<engine_skin_component_t>();
        skin_updated_view.each([this](engine_skin_component_t& skin_component)
            {
                if (!skin_component.skin)
                {
                    log::log(log::LogLevel::eError, fmt::format("Skin component has no skin assigned. Are you sure you are doing valid thing?\n"));
                }
                auto typed_skin = reinterpret_cast<engine::Skin*>(skin_component.skin);
                
            });
    }

    {
        ENGINE_PROFILE_SECTION_N("parent_to_child_transform_view");
        std::map<entt::entity, glm::mat4> ltw_map;
        //ToDo: this coule be optimized if entityies are sorted, so parents are always computed first
        auto parent_to_child_transform_view = entity_registry_.view<engine_tranform_component_t, const engine_parent_component_t>();
        parent_to_child_transform_view.each([this, &ltw_map](auto entity, engine_tranform_component_t& transform_comp, const engine_parent_component_t& parent_comp)
            {
                //engine::log::log(engine::log::LogLevel::eTrace, fmt::format("updating ent: {}\n", static_cast<std::uint32_t>(entity)));
                auto ltw_matrix = glm::make_mat4(transform_comp.local_to_world);
                auto parent = parent_comp.parent;
                while (parent != ENGINE_INVALID_GAME_OBJECT_ID)
                {
                    const auto parent_entt = static_cast<entt::entity>(parent);
                    if (has_component<engine_tranform_component_t>(parent_entt))
                    {
                        const auto parent_ltw_matrix = get_component<engine_tranform_component_t>(parent_entt);
                        ltw_matrix = glm::make_mat4(parent_ltw_matrix->local_to_world) * ltw_matrix;
                        if (has_component<engine_parent_component_t>(parent_entt))
                        {
                            const auto pc = get_component<engine_parent_component_t>(parent_entt);
                            parent = pc->parent;
                        }
                        else
                        {
                            // break the recussion
                            parent = ENGINE_INVALID_GAME_OBJECT_ID;
                        }
                    }
                    else
                    {
                        // break the recussion
                        parent = ENGINE_INVALID_GAME_OBJECT_ID;
                    }
                    ltw_map[entity] = ltw_matrix;
                }
            });
        {
            ENGINE_PROFILE_SECTION_N("update_ltws_with_parents");
            for (const auto& [entity, ltw_matrix] : ltw_map)
            {
                auto transform_comp = *get_component<engine_tranform_component_t>(entity);
                std::memcpy(transform_comp.local_to_world, &ltw_matrix, sizeof(ltw_matrix));
                update_component(entity, transform_comp);
            }
        }

    }

    std::uint32_t directional_light_count = 0;
    std::uint32_t point_light_count = 0;
    std::uint32_t spot_light_count = 0;
    // copy lights data to the GPU
    {
        ENGINE_PROFILE_SECTION_N("lights update");
        auto lights_view = entity_registry_.view<const engine_tranform_component_t, const engine_light_component_t>();
        {
            ENGINE_PROFILE_SECTION_N("lights counter");
            lights_view.each([&directional_light_count, &point_light_count, &spot_light_count](const engine_tranform_component_t& transform, const engine_light_component_t& light)
                {
                    if (light.type == ENGINE_LIGHT_TYPE_DIRECTIONAL)
                    {
                        directional_light_count++;
                    }
                    else if (light.type == ENGINE_LIGHT_TYPE_POINT)
                    {
                        point_light_count++;
                    }
                    else if (light.type == ENGINE_LIGHT_TYPE_SPOT)
                    {
                        spot_light_count++;
                    }
                });
        }

        {
            ENGINE_PROFILE_SECTION_N("lights_ssbo update");
            const auto total_lights = directional_light_count + point_light_count + spot_light_count;
            if (total_lights > light_data_ssbo_.get_size())
            {
                log::log(log::LogLevel::eTrace, fmt::format("Light data SSBO is too small. Increasing the size of the buffer. Current size: {}. Required size: {}\n", light_data_ssbo_.get_size(), total_lights));
                light_data_ssbo_ = ShaderStorageBuffer(total_lights * sizeof(LightGpuData));
            }
            BufferMapContext<LightGpuData, ShaderStorageBuffer> light_data(light_data_ssbo_, false, true);
            std::int32_t dir_idx = 0;
            std::int32_t point_idx = directional_light_count;
            std::int32_t spot_idx = directional_light_count + point_light_count;
            lights_view.each([&light_data, &dir_idx, &point_idx, &spot_idx](const engine_tranform_component_t& transform, const engine_light_component_t& light)
                {
                    LightGpuData* light_data_ptr = nullptr;;
                    if (light.type == ENGINE_LIGHT_TYPE_DIRECTIONAL)
                    {
                        light_data_ptr = &light_data.data[dir_idx++];
                        light_data_ptr->direction = glm::make_vec3(light.directional.direction);
                    }
                    else if (light.type == ENGINE_LIGHT_TYPE_POINT)
                    {
                        light_data_ptr = &light_data.data[point_idx++];
                        light_data_ptr->position = glm::make_vec3(transform.position);
                        light_data_ptr->constant = light.point.constant;
                        light_data_ptr->linear = light.point.linear;
                        light_data_ptr->quadratic = light.point.quadratic;
                    }
                    else if (light.type == ENGINE_LIGHT_TYPE_SPOT)
                    {
                        light_data_ptr = &light_data.data[spot_idx++];
                        light_data_ptr->position = glm::make_vec3(transform.position);
                        light_data_ptr->direction = glm::make_vec3(light.spot.direction);
                        light_data_ptr->cutoff = glm::cos(glm::radians(light.spot.cut_off));
                        light_data_ptr->outer_cutoff = glm::cos(glm::radians(light.spot.outer_cut_off));
                        light_data_ptr->constant = light.spot.constant;
                        light_data_ptr->linear = light.spot.linear;
                        light_data_ptr->quadratic = light.spot.quadratic;
                    }
                    assert(light_data_ptr);
                    light_data_ptr->ambient = glm::make_vec3(light.intensity.ambient);
                    light_data_ptr->diffuse = glm::make_vec3(light.intensity.diffuse);
                    light_data_ptr->specular = glm::make_vec3(light.intensity.specular);
                });
            light_data.unmap();
            light_data_ssbo_.bind(2);

            assert(dir_idx == directional_light_count);
            assert(point_idx == directional_light_count + point_light_count);
            assert(spot_idx == directional_light_count + point_light_count + spot_light_count);
        }

    }

    {
        ENGINE_PROFILE_SECTION_N("scene_ubo update");
        BufferMapContext<SceneGpuData, UniformBuffer> scene_ubo(scene_ubo_, false, true);
        scene_ubo.data->direction_light_count = directional_light_count;
        scene_ubo.data->point_light_count = point_light_count;
        scene_ubo.data->spot_light_count = spot_light_count;
        scene_ubo.unmap();
    }

    {
        ENGINE_PROFILE_SECTION_N("camera_loop");

        auto mesh_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t>(entt::exclude<engine_skin_component_t>);
        auto skinned_mesh_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_skinned_mesh_component_t, const engine_material_component_t>();
        auto geometry_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t>(entt::exclude<engine_skin_component_t>);
        auto skinned_geometry_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, engine_skin_component_t, const engine_material_component_t>();
        auto sprite_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_material_component_t, const engine_sprite_component_t>();
        auto camera_view = entity_registry_.view<const engine_camera_component_t, const engine_tranform_component_t, camera_internal_component_t>();

        for (auto [entity, camera, camera_transform, camera_internal] : camera_view.each()) 
        {
            if (!camera.enabled)
            {
                continue;
            }

            // bind default framebuffer
            const auto window_size_pixels = rdx_.get_window_size_in_pixels();
            viewport_t vp{};
            vp.x = 0;
            vp.y = 0;
            vp.width = window_size_pixels.width;
            vp.height = window_size_pixels.height;
            rdx_.set_viewport(vp);

            calculate_camera_view_and_projection(window_size_pixels.width, window_size_pixels.height,
                glm::make_vec3(camera_transform.position), camera, camera_internal);
            // copy camera view and projection to the GPU
            {
                ENGINE_PROFILE_SECTION_N("camera_ubo_update");
                BufferMapContext<CameraGpuData, UniformBuffer> camera_ubo(camera_internal.camera_ubo, false, true);
                std::memcpy(camera_ubo.data, &camera_internal.data, sizeof(CameraGpuData));
                // already updated GPU buffer to mark this camera as not computed
                camera_internal.computed_this_frame = false; 
            }

            {
                ENGINE_PROFILE_SECTION_N("geometry_renderer");

                mesh_renderer.each([this, &camera_internal](auto entity, const engine_tranform_component_t& transform_component, const engine_mesh_component_t& mesh_component, const engine_material_component_t& material_component)
                    {
                        if (mesh_component.disable)
                        {
                            return;
                        }
                        if (mesh_component.geometry == ENGINE_INVALID_OBJECT_HANDLE)
                        {
                            log::log(log::LogLevel::eError, fmt::format("[Entity: {}]. Mesh component has invalid geometry handle. Are you sure you are doing valid thing?\n", (std::uint32_t)entity));
                            return;
                        }

                        const auto texture_diffuse_idx = material_component.data.pong.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.data.pong.diffuse_texture;
                        const auto texture_specular_idx = material_component.data.pong.specular_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.data.pong.specular_texture;

                        const auto tex2d_diffuse_ptr = get_application()->get_texture(texture_diffuse_idx);
                        const auto tex2d_specular_ptr = get_application()->get_texture(texture_diffuse_idx);

                        const auto ctx = MaterialStaticGeometryLit::DrawContext{
                            .entity_id = static_cast<std::uint32_t>(entity),
                            .camera = camera_internal.camera_ubo,
                            .scene = scene_ubo_,
                            .model_matrix = transform_component.local_to_world,
                            .color_diffuse = material_component.data.pong.diffuse_color,
                            .shininess = static_cast<float>(material_component.data.pong.shininess),     
                            .texture_diffuse = *tex2d_diffuse_ptr,
                            .texture_specular = *tex2d_specular_ptr };

                        const auto geo_ptr = get_application()->get_geometry(mesh_component.geometry);
                        material_static_geometry_lit_.draw(*geo_ptr, ctx);
                    }
                );
            }

            {
                ENGINE_PROFILE_SECTION_N("skinned_mesh_renderer");

                skinned_mesh_renderer.each([this, &camera_internal](auto entity, const engine_tranform_component_t& transform_component, const engine_skinned_mesh_component_t& skinned_mesh_component, const engine_material_component_t& material_component)
                    {
                        if (skinned_mesh_component.disable)
                        {
                            return;
                        }
                        if (skinned_mesh_component.geometry == ENGINE_INVALID_OBJECT_HANDLE)
                        {
                            log::log(log::LogLevel::eError, fmt::format("[Entity: {}]. Mesh component has invalid geometry handle. Are you sure you are doing valid thing?\n", (std::uint32_t)entity));
                            return;
                        }

                        if (!skinned_mesh_component.skin)
                        {
                            log::log(log::LogLevel::eError, fmt::format("[Entity: {}]. Skinned mesh component has no skin assigned. Are you sure you are doing valid thing?\n", (std::uint32_t)entity));
                            return;
                        }

                        const auto texture_diffuse_idx = material_component.data.pong.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.data.pong.diffuse_texture;
                        const auto texture_specular_idx = material_component.data.pong.specular_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.data.pong.specular_texture;

                        const auto tex2d_diffuse_ptr = get_application()->get_texture(texture_diffuse_idx);
                        const auto tex2d_specular_ptr = get_application()->get_texture(texture_diffuse_idx);

#if 1
                        auto ctx = MaterialSkinnedGeometryLit::DrawContext{
                            .entity_id = static_cast<std::uint32_t>(entity),
                            .camera = camera_internal.camera_ubo,
                            .scene = scene_ubo_,
                            .model_matrix = transform_component.local_to_world,
                            .color_diffuse = material_component.data.pong.diffuse_color,
                            .shininess = material_component.data.pong.shininess,
                            .texture_diffuse = *tex2d_diffuse_ptr,
                            .texture_specular = *tex2d_specular_ptr };
                        const auto typed_skin = reinterpret_cast<engine::Skin*>(skinned_mesh_component.skin);
                        ctx.bone_transforms = typed_skin->get_skinning_matrices();

                        const auto geo_ptr = get_application()->get_geometry(skinned_mesh_component.geometry);
                        material_skinned_geometry_lit_.draw(*geo_ptr, ctx);
#else
                        const auto ctx = MaterialStaticGeometryLit::DrawContext{
                            .entity_id = static_cast<std::uint32_t>(entity),
                            .camera = camera_internal.camera_ubo,
                            .scene = scene_ubo_,
                            .model_matrix = transform_component.local_to_world,
                            .color_diffuse = material_component.data.pong.diffuse_color,
                            .shininess = static_cast<float>(material_component.data.pong.shininess),
                            .texture_diffuse = *tex2d_diffuse_ptr,
                            .texture_specular = *tex2d_specular_ptr };

                        const auto geo_ptr = get_application()->get_geometry(skinned_mesh_component.geometry);
                        material_static_geometry_lit_.draw(*geo_ptr, ctx);
#endif
                    }
                );
            }

            //{
            //    ENGINE_PROFILE_SECTION_N("skinned_geometry_renderer");

            //    skinned_geometry_renderer.each([this, &camera_internal](auto entity, const engine_tranform_component_t& transform_component, const engine_mesh_component_t& mesh_component,
            //        engine_skin_component_t& skin_component, const engine_material_component_t& material_component)
            //        {
            //            if (mesh_component.disable)
            //            {
            //                return;
            //            }

            //            const auto texture_diffuse_idx = material_component.data.pong.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.data.pong.diffuse_texture;
            //            const auto texture_specular_idx = material_component.data.pong.specular_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.data.pong.specular_texture;

            //            const auto tex2d_diffuse_ptr = get_application()->get_texture(texture_diffuse_idx);
            //            const auto tex2d_specular_ptr = get_application()->get_texture(texture_diffuse_idx);

            //            auto ctx = MaterialSkinnedGeometryLit::DrawContext{
            //                .entity_id = static_cast<std::uint32_t>(entity),
            //                .camera = camera_internal.camera_ubo,
            //                .scene = scene_ubo_,
            //                .model_matrix = transform_component.local_to_world,
            //                .color_diffuse = material_component.data.pong.diffuse_color,
            //                .shininess = material_component.data.pong.shininess,
            //                .texture_diffuse = *tex2d_diffuse_ptr,
            //                .texture_specular = *tex2d_specular_ptr };
            //            ctx.bone_transforms.reserve(ENGINE_SKINNED_MESH_COMPONENT_MAX_SKELETON_BONES); // reallocation this for each geometry each frame. ToDo: optimize it

            //            const auto inverse_transform = glm::inverse(glm::make_mat4(transform_component.local_to_world));
            //            for (std::size_t i = 0; i < ENGINE_SKINNED_MESH_COMPONENT_MAX_SKELETON_BONES; i++)
            //            {
            //                const auto& bone_entity = static_cast<entt::entity>(skin_component.bones[i]);
            //                if (static_cast<std::uint32_t>(bone_entity) == ENGINE_INVALID_GAME_OBJECT_ID)
            //                {
            //                    continue;
            //                }

            //                if (has_component<engine_bone_component_t>(bone_entity) == false)
            //                {
            //                    log::log(log::LogLevel::eError, fmt::format("Bone entity does not have bone component. Are you sure you are doing valid thing?\n"));
            //                    skin_component.bones[i] = ENGINE_INVALID_GAME_OBJECT_ID;
            //                    continue;
            //                }
            //                const auto& bone_component = get_component<engine_bone_component_t>(bone_entity);
            //                const auto& bone_transform = get_component<engine_tranform_component_t>(bone_entity);
            //                const auto inverse_bind_matrix = glm::make_mat4(bone_component->inverse_bind_matrix);
            //                const auto bone_matrix = glm::make_mat4(bone_transform->local_to_world) * inverse_bind_matrix;
            //                const auto per_bone_final_transform = inverse_transform * bone_matrix;
            //                ctx.bone_transforms.push_back(per_bone_final_transform);
            //            }
            //            const auto geo_ptr = get_application()->get_geometry(mesh_component.geometry);
            //            material_skinned_geometry_lit_.draw(*geo_ptr, ctx);

            //        }
            //    );
            //}

            {
                ENGINE_PROFILE_SECTION_N("sprite_renderer");

                sprite_renderer.each([this, &camera_internal](const engine_tranform_component_t& transform_component, const engine_material_component_t& material_component, const engine_sprite_component_t& sprite_component)
                    {

                        auto model_mat = glm::make_mat4(transform_component.local_to_world);
                        glm::vec3 scale;
                        glm::quat rotation;
                        glm::vec3 translation;
                        glm::vec3 skew;
                        glm::vec4 perspective;
                        const auto res = glm::decompose(model_mat, scale, rotation, translation, skew, perspective);
                        assert(res);

                        if (material_component.type == ENGINE_MATERIAL_TYPE_PONG)
                        {
                            const auto ctx = MaterialSprite::DrawContext{
                                .camera = camera_internal.camera_ubo,
                                .scene = scene_ubo_,
                                .world_position = translation,
                                .scale = scale };
                            material_sprite_.draw(ctx);
                        }
                        else
                        {
                            assert(false);
                        }
                    }
                );
            }

            if (physics_world_.is_debug_drawer_enabled())
            {
                physics_world_.debug_draw(camera_internal.data.view, camera_internal.data.projection);
            }
        }

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

std::vector<entt::entity> engine::Scene::get_all_entities() const
{
    std::vector<entt::entity> entities;
    for (const auto entity : entity_registry_.view<entt::entity>())
    {
        if (static_cast<std::uint32_t>(entity) != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            entities.push_back(entity);
        }
    }
    return entities;
}

engine::Skin* engine::Scene::create_skin(const engine_vector_uint32_t& bones, const char* name)
{
    if (bones->data.empty())
    {
        return nullptr;
    }
    //ToDo: modify this, so user pass only root bone and other bones are recurisvly added by getting parent/children components

    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    raw_skeleton.roots[0].name = "root";

    //for (std::size_t i = 0; i < bones->data.size(); ++i)
    //{
    //    const auto entity = static_cast<entt::entity>(bones->data[i]);
    //    const auto& tc = get_component<engine_tranform_component_t>(entity);
    //    if (has_component<engine_parent_component_t>(entity))
    //    {
    //        const auto& pc = get_component<engine_parent_component_t>(entity);
    //        if()
    //    }


    //    ozz::animation::offline::RawSkeleton::Joint joint;
    //    joint.name = fmt::format("bone_{}", i);
    //    joint.parent = (i == 0) ? -1 : static_cast<int>(i - 1);
    //    raw_skeleton.joints.push_back(joint);
    //}

    return nullptr;
}

void engine::Scene::set_physcis_gravity(std::array<float, 3> g)
{
    physics_world_.set_gravity(g);
}

const std::vector<engine::CollisionDesc>& engine::Scene::get_physcis_collisions() const
{
    return physics_world_.get_collisions();
}

engine_ray_hit_info_t engine::Scene::raycast_into_physics_world(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance)
{
    return physics_world_.raycast(ray, ignore_list, max_distance);
}

bool engine::Scene::add_force_to_physics_entity(entt::entity entity, std::array<float, 3> force, engine_force_type_t type)
{
    /*
    Deffered force application to the physics world. Beacuse at this point rigid body may not be created yet (i.e. force applied immeditly after creating object).
    */
    if (has_component<physcic_internal_component_t>(entity))
    {
        auto& physcics_component = entity_registry_.get<physcic_internal_component_t>(entity);
        if (type == engine_force_type_t::ENGINE_FORCE_TYPE_FORCE)
        {
            forces_to_apply_.push_back({ entity, force, type });
        }
        else if (type == engine_force_type_t::ENGINE_FORCE_TYPE_IMPLUSE)
        {
            forces_to_apply_.push_back({ entity, force, type });
        }
        else
        {
            log::log(log::LogLevel::eError, fmt::format("Unknown force type: {}\n", static_cast<std::uint32_t>(type)));
            return false;
        }
        return true;
    }
    return false;
}

glm::vec3 engine::Scene::convert_world_point_to_screen_point(const glm::vec3& world_point, engine_game_object_t camera_go)
{
    auto ret = glm::vec3(0.5f, 0.5f, 0.0f);
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(entt::entity(camera_go));
    const auto camera_transform = entity_registry_.try_get<engine_tranform_component_t>(entt::entity(camera_go));
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(entt::entity(camera_go));
        const auto window_size = rdx_.get_window_size_in_pixels();
        calculate_camera_view_and_projection(window_size.width, window_size.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);
        ret = glm::project(world_point, camera_internal_component.data.view, camera_internal_component.data.projection, glm::vec4(0, 0, window_size.width, window_size.height));
        ret.x /= window_size.width;
        ret.y /= window_size.height;
    }
    return ret;
}

glm::vec3 engine::Scene::convert_screen_point_to_world_point(glm::vec3 screen_point, engine_game_object_t camera_go)
{
    auto ret = glm::vec3(0.0f, 0.0f, 0.0f);
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(entt::entity(camera_go));
    const auto camera_transform = entity_registry_.try_get<engine_tranform_component_t>(entt::entity(camera_go));
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(entt::entity(camera_go));    
        const auto window_size = rdx_.get_window_size_in_pixels();
        calculate_camera_view_and_projection(window_size.width, window_size.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);

        const float aspect = window_size.width / window_size.height;
        screen_point.x *= window_size.width;
        screen_point.y *= window_size.height;
        ret = glm::unProject(screen_point, camera_internal_component.data.view, camera_internal_component.data.projection, glm::vec4(0, 0, window_size.width, window_size.height));
    }
    return ret;
}

glm::mat4 engine::Scene::get_camera_view(entt::entity camera_go)
{
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(camera_go);
    const auto camera_transform = entity_registry_.try_get<engine_tranform_component_t>(camera_go);
    const auto window_size = rdx_.get_window_size_in_pixels();
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(camera_go);
        calculate_camera_view_and_projection(window_size.width, window_size.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);
        return camera_internal_component.data.view;
    }
    return glm::mat4();
}

glm::mat4 engine::Scene::get_camera_projection(entt::entity camera_go)
{
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(camera_go);
    const auto camera_transform = entity_registry_.try_get<engine_tranform_component_t>(camera_go);
    const auto window_size = rdx_.get_window_size_in_pixels();
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(camera_go);
        calculate_camera_view_and_projection(window_size.width, window_size.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);
        return camera_internal_component.data.projection;
    }
    return glm::mat4();
}

