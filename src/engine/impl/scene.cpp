#include "scene.h"
#include "ui_manager.h"
#include "logger.h"
#include "math_helpers.h"
#include "components_utils/components_initializers.h"
#include "profiler.h"


#include <fmt/format.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <SDL3/SDL.h>

#include <RmlUi/Core.h>


void update_parent_component(entt::registry& registry, entt::entity entity)
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

struct CameraGpuData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 position;
};

struct engine_camera_internal_component_t
{
    engine::UniformBuffer camera_ubo = engine::UniformBuffer(sizeof(CameraGpuData));
};



engine::Scene::Scene(RenderContext& rdx, const engine_scene_create_desc_t& config, engine_result_code_t& out_code)
    : rdx_(rdx)
    , physics_world_(&rdx_)
    , fbo_(rdx.get_window_size_in_pixels().width, rdx.get_window_size_in_pixels().height, 1, true)
    , empty_vao_for_full_screen_quad_draw_(6)
    , collider_create_observer(entity_registry_, entt::collector.group<engine_tranform_component_t, engine_collider_component_t>(entt::exclude<engine_rigid_body_component_t>))
    , collider_update_observer(entity_registry_, entt::collector.update<engine_collider_component_t>().where<engine_tranform_component_t>())
    , transform_update_collider_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>().where<PhysicsWorld::physcic_internal_component_t>())
    , transform_model_matrix_update_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>())
    , mesh_update_observer(entity_registry_, entt::collector.update<engine_mesh_component_t>())
    , rigid_body_create_observer(entity_registry_, entt::collector.group<engine_rigid_body_component_t, engine_tranform_component_t, engine_collider_component_t>())
    , rigid_body_update_observer(entity_registry_, entt::collector.update<engine_rigid_body_component_t>().where<engine_tranform_component_t, engine_collider_component_t>())
    , scene_ubo_(sizeof(SceneGpuData))
    , light_data_ssbo_(1'000 * sizeof(LightGpuData))
{
    // shaders
    shaders_[static_cast<std::uint32_t>(ShaderType::eUnlit)] = Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "unlit.fs" });
    shaders_[static_cast<std::uint32_t>(ShaderType::eLit)] = Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "lit.fs" });
    shaders_[static_cast<std::uint32_t>(ShaderType::eVertexSkinningUnlit)] = Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "unlit.fs" });
    shaders_[static_cast<std::uint32_t>(ShaderType::eVertexSkinningLit)] = Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "lit.fs" });
    shaders_[static_cast<std::uint32_t>(ShaderType::eFullScreenQuad)] = Shader({ "full_screen_quad.vs" }, { "full_screen_quad.fs" });

    // basic initalizers
    entity_registry_.on_construct<engine_tranform_component_t>().connect<&initialize_transform_component>();
    entity_registry_.on_construct<engine_mesh_component_t>().connect<&initialize_mesh_component>();
    entity_registry_.on_construct<engine_material_component_t>().connect<&initialize_material_component>();
    entity_registry_.on_construct<engine_parent_component_t>().connect<&initialize_parent_component>();
    entity_registry_.on_construct<engine_name_component_t>().connect<&initialize_name_component>();
    entity_registry_.on_construct<engine_camera_component_t>().connect<&initialize_camera_component>();
    entity_registry_.on_construct<engine_camera_component_t>().connect<&entt::registry::emplace<engine_camera_internal_component_t>>();
    entity_registry_.on_construct<engine_rigid_body_component_t>().connect<&initialize_rigidbody_component>();
    entity_registry_.on_construct<engine_collider_component_t>().connect<&initialize_collider_component>();
    entity_registry_.on_construct<engine_skin_component_t>().connect<&initialize_skin_component>();
    entity_registry_.on_construct<engine_light_component_t>().connect<&initialize_light_component>();
    
    entity_registry_.on_update<engine_parent_component_t>().connect<&update_parent_component>();
    entity_registry_.on_construct<engine_collider_component_t>().connect<&entt::registry::emplace<PhysicsWorld::physcic_internal_component_t>>();
    entity_registry_.on_destroy<engine_collider_component_t>().connect<&entt::registry::remove<PhysicsWorld::physcic_internal_component_t>>();
    entity_registry_.on_destroy<PhysicsWorld::physcic_internal_component_t>().connect<&PhysicsWorld::remove_rigid_body>(&physics_world_);
    out_code = ENGINE_RESULT_CODE_OK;

    
}

engine::Scene::~Scene()
{  
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
        auto physcics_component = *get_component<PhysicsWorld::physcic_internal_component_t>(entt);
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
        auto physcics_component = *get_component< PhysicsWorld::physcic_internal_component_t>(entt);
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
        auto physcics_component = *get_component< PhysicsWorld::physcic_internal_component_t>(entt);

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
            auto& physcics_component = *get_component<PhysicsWorld::physcic_internal_component_t>(entity);
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

    physics_world_.update(dt / 1000.0f);

    // sync physcis to graphics world
    // ToDo: this could be seperate function or called at the beggning of the graphics update function?
    auto transform_physcis_view_post_update = entity_registry_.view<engine_tranform_component_t, const PhysicsWorld::physcic_internal_component_t, engine_rigid_body_component_t>();
    transform_physcis_view_post_update.each([this](auto entity, engine_tranform_component_t& transform, const PhysicsWorld::physcic_internal_component_t& physcics, engine_rigid_body_component_t& rigidbody)
        {
            //assert(physcics.rigid_body);
            if (!physcics.rigid_body)
            {
                return;
            }
            btTransform transform_phsycics{};
            transform_phsycics = physcics.rigid_body->getWorldTransform();
            //physcics.rigid_body->getMotionState()->getWorldTransform(transform_phsycics);   

            const auto origin = transform_phsycics.getOrigin();
            transform.position[0] = origin.getX();
            transform.position[1] = origin.getY();
            transform.position[2] = origin.getZ();

            const auto euler_rotation = transform_phsycics.getRotation();
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

engine_result_code_t engine::Scene::update(float dt, std::span<const Texture2D> textures, 
    std::span<const Geometry> geometries, std::span<const engine_material_create_desc_t> materials)
{
    ENGINE_PROFILE_SECTION_N("scene_update");
    physics_update(dt);
    class FBOFrameContext
    {
    public:
        FBOFrameContext(Framebuffer& fbo, const RenderContext& rdx, Shader& full_screen_quad, Geometry& empty_vao)
            : fbo_(fbo)
            , full_screen_quad_shader_(full_screen_quad)
            , empty_vao(empty_vao)
        {
            fbo_.bind();
            const auto& [fbo_w, fbo_h] = fbo_.get_size();
            const auto& [win_w, win_h] = rdx.get_window_size_in_pixels();
            if (fbo_w != win_w || fbo_h != win_h)
            {
                fbo_.resize(win_w, win_h);
            }
            fbo_.clear();
        }

        ~FBOFrameContext()
        {
            fbo_.unbind();
            full_screen_quad_shader_.bind();
            full_screen_quad_shader_.set_texture("screen_texture", fbo_.get_color_attachment(0));
            empty_vao.bind();
            empty_vao.draw(Geometry::Mode::eTriangles);
        }

    private:
        Framebuffer& fbo_;
        Shader& full_screen_quad_shader_;
        Geometry& empty_vao;
    };
    FBOFrameContext fbo_frame(fbo_, rdx_, shaders_[static_cast<std::uint32_t>(ShaderType::eFullScreenQuad)], empty_vao_for_full_screen_quad_draw_);
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
                ltw_map[entity] = ltw_matrix;
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
            lights_view.each([&light_data](const engine_tranform_component_t& transform, const engine_light_component_t& light)
                {
                    if (light.type == ENGINE_LIGHT_TYPE_DIRECTIONAL)
                    {
                        light_data.data->direction = glm::make_vec3(light.directional.direction);
                    }
                    light_data.data->ambient = glm::make_vec3(light.intensity.ambient);
                    light_data.data->diffuse = glm::make_vec3(light.intensity.diffuse);
                    light_data.data->specular = glm::make_vec3(light.intensity.specular);
                    light_data.data++;
                });
            light_data.unmap();
            light_data_ssbo_.bind(2);
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

        auto geometry_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t>(entt::exclude<engine_skin_component_t>);
        auto skinned_geometry_renderer = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, engine_skin_component_t, const engine_material_component_t>();
        auto camera_view = entity_registry_.view<const engine_camera_component_t, const engine_tranform_component_t, engine_camera_internal_component_t>();

        for (auto [entity, camera, camera_transform, camera_internal] : camera_view.each()) 
        {
            if (!camera.enabled)
            {
                continue;
            }

            const auto window_size_pixels = rdx_.get_window_size_in_pixels();

            glm::mat4 view = glm::mat4(0.0);
            glm::mat4 projection = glm::mat4(0.0);
            // update camera: view and projection
            {
                ENGINE_PROFILE_SECTION_N("camera_update");
                const auto z_near = camera.clip_plane_near;
                const auto z_far = camera.clip_plane_far;
                // ToD: multi camera - this should use resolution of camera!!!

                const auto adjusted_width = window_size_pixels.width * (camera.viewport_rect.width - camera.viewport_rect.x);
                const auto adjusted_height = window_size_pixels.height * (camera.viewport_rect.height - camera.viewport_rect.y);
                const float aspect = adjusted_width / adjusted_height;

                if (camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC)
                {
                    const float scale = camera.type_union.orthographics_scale;
                    projection = glm::ortho(-aspect * scale, aspect * scale, -scale, scale, z_near, z_far);
                }
                else
                {
                    projection = glm::perspective(glm::radians(camera.type_union.perspective_fov), aspect, z_near, z_far);
                }
                const auto eye_position = glm::make_vec3(camera_transform.position);
                const auto up = glm::make_vec3(camera.direction.up);
                const auto target = glm::make_vec3(camera.target);
                view = glm::lookAt(eye_position, target, up);
            }

            // copy camera view and projection to the GPU
            {
                ENGINE_PROFILE_SECTION_N("camera_ubo_update");
                BufferMapContext<CameraGpuData, UniformBuffer> camera_ubo(camera_internal.camera_ubo, false, true);
                camera_ubo.data->view = view;
                camera_ubo.data->projection = projection;
                camera_ubo.data->position = glm::make_vec3(camera_transform.position);
            }


            {
                ENGINE_PROFILE_SECTION_N("geometry_renderer");

                geometry_renderer.each([this, &camera_internal, &textures, &geometries, &materials](const engine_tranform_component_t& transform_component, const engine_mesh_component_t& mesh_component, const engine_material_component_t& material_component)
                    {
                        if (mesh_component.disable)
                        {
                            return;
                        }
                        if (mesh_component.geometry == ENGINE_INVALID_OBJECT_HANDLE)
                        {
                            log::log(log::LogLevel::eError, fmt::format("Mesh component has invalid geometry handle. Are you sure you are doing valid thing?\n"));
                            return;
                        }
                        const auto& material = materials[material_component.material == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.material];
                        
                        const auto shader_type = ShaderType::eLit;
                        auto& shader = shaders_[static_cast<std::uint32_t>(shader_type)];
                        shader.bind();

                        shader.set_uniform_block("CameraData", &camera_internal.camera_ubo, 0);    
                        shader.set_uniform_mat_f4("model", transform_component.local_to_world);                       

                        auto texture_diffuse_idx = material.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material.diffuse_texture;
                        if (texture_diffuse_idx > textures.size())
                        {
                            log::log(log::LogLevel::eError, fmt::format("Texture index out of bounds: {}. Are you sure you are doing valid thing?\n", texture_diffuse_idx));
                            assert(false);
                            texture_diffuse_idx = 0;
                        }
                        shader.set_uniform_f3("diffuse_color", material.diffuse_color);
                        shader.set_texture("texture_diffuse", &textures[texture_diffuse_idx]);

                        if (shader_type == ShaderType::eLit)
                        {
                            shader.set_uniform_block("SceneData", &scene_ubo_, 1);
                            shader.set_uniform_f1("shininess", static_cast<float>(material.shininess));
                            const auto texture_specular_idx = material.specular_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material.specular_texture;
                            shader.set_texture("texture_specular", &textures[texture_specular_idx]);
                        }

                        geometries[mesh_component.geometry].bind();
                        geometries[mesh_component.geometry].draw(Geometry::Mode::eTriangles);

                    }
                );
            }

            {
                ENGINE_PROFILE_SECTION_N("skinned_geometry_renderer");

                skinned_geometry_renderer.each([this, &camera_internal, &textures, &geometries, &materials](auto entity, const engine_tranform_component_t& transform_component, const engine_mesh_component_t& mesh_component,
                    engine_skin_component_t& skin_component, const engine_material_component_t& material_component)
                    {
                        if (mesh_component.disable)
                        {
                            return;
                        }
                        const auto& material = materials[material_component.material == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.material];
                        
                        const auto shader_type = ShaderType::eVertexSkinningLit;
                        auto& shader = shaders_[static_cast<std::uint32_t>(shader_type)];
                        shader.bind();
                        shader.set_uniform_block("CameraData", &camera_internal.camera_ubo, 0);
                        shader.set_uniform_mat_f4("model", transform_component.local_to_world);

                        const auto texture_diffuse_idx = material.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material.diffuse_texture;
                        shader.set_texture("texture_diffuse", &textures[texture_diffuse_idx]);
                        shader.set_uniform_f3("diffuse_color", material.diffuse_color);

                        if (shader_type == ShaderType::eVertexSkinningLit)
                        {
                            shader.set_uniform_block("SceneData", &scene_ubo_, 1);
                            shader.set_uniform_f1("shininess", static_cast<float>(material.shininess));
                            const auto texture_specular_idx = material.specular_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material.specular_texture;
                            shader.set_texture("texture_specular", &textures[texture_specular_idx]);
                        }


                        const auto inverse_transform = glm::inverse(glm::make_mat4(transform_component.local_to_world));
                        for (std::size_t i = 0; i < ENGINE_SKINNED_MESH_COMPONENT_MAX_SKELETON_BONES; i++)
                        {
                            const auto& bone_entity = static_cast<entt::entity>(skin_component.bones[i]);
                            if (static_cast<std::uint32_t>(bone_entity) == ENGINE_INVALID_GAME_OBJECT_ID)
                            {
                                continue;
                            }

                            if (has_component<engine_bone_component_t>(bone_entity) == false)
                            {
                                log::log(log::LogLevel::eError, fmt::format("Bone entity does not have bone component. Are you sure you are doing valid thing?\n"));
                                skin_component.bones[i] = ENGINE_INVALID_GAME_OBJECT_ID;
                                continue;
                            }
                            const auto& bone_component = get_component<engine_bone_component_t>(bone_entity);
                            const auto& bone_transform = get_component<engine_tranform_component_t>(bone_entity);
                            const auto inverse_bind_matrix = glm::make_mat4(bone_component->inverse_bind_matrix);
                            const auto bone_matrix = glm::make_mat4(bone_transform->local_to_world) * inverse_bind_matrix;
                            const auto per_bone_final_transform = inverse_transform * bone_matrix;
                            const auto uniform_name = "global_bone_transform[" + std::to_string(i) + "]";
                            shader.set_uniform_mat_f4(uniform_name, { glm::value_ptr(per_bone_final_transform), sizeof(per_bone_final_transform) / sizeof(float) });
                        }

                        geometries[mesh_component.geometry].bind();
                        geometries[mesh_component.geometry].draw(Geometry::Mode::eTriangles);

                    }
                );
            }

            if (physics_world_.is_debug_drawer_enabled())
            {
                physics_world_.debug_draw(view, projection);
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

engine_ray_hit_info_t engine::Scene::raycast_into_physics_world(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance)
{
    return physics_world_.raycast(ray, ignore_list, max_distance);
}

