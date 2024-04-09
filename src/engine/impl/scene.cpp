#include "scene.h"
#include "animation.h"
#include "ui_manager.h"
#include "logger.h"
#include "math_helpers.h"
#include "vertex_skinning.h"

#include <fmt/format.h>


#include <SDL3/SDL.h>

#include <RmlUi/Core.h>


engine::Scene::Scene(engine_result_code_t& out_code)
    : shader_simple_(Shader("simple.vs", "simple.fs"))
    , shader_vertex_skinning_(Shader("vertex_skinning.vs", "simple.fs"))
    , collider_create_observer(entity_registry_, entt::collector.group<engine_tranform_component_t, engine_collider_component_t>(entt::exclude<engine_rigid_body_component_t>))
    , transform_update_collider_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>().where<PhysicsWorld::physcic_internal_component_t>())
    , transform_model_matrix_update_observer(entity_registry_, entt::collector.update<engine_tranform_component_t>())
    , mesh_update_observer(entity_registry_, entt::collector.update<engine_mesh_component_t>())
    , rigid_body_create_observer(entity_registry_, entt::collector.group<engine_rigid_body_component_t, engine_tranform_component_t, engine_collider_component_t>())
    , rigid_body_update_observer(entity_registry_, entt::collector.update<engine_rigid_body_component_t>().where<engine_tranform_component_t, engine_collider_component_t>())
{
    entity_registry_.on_construct<engine_mesh_component_t>().connect<&entt::registry::emplace<engine_skin_internal_component_t>>();
    entity_registry_.on_construct<engine_collider_component_t>().connect<&entt::registry::emplace<PhysicsWorld::physcic_internal_component_t>>();
    entity_registry_.on_destroy<PhysicsWorld::physcic_internal_component_t>().connect<&PhysicsWorld::remove_rigid_body>(&physics_world_);
    out_code = ENGINE_RESULT_CODE_OK;

    physics_world_.enable_debug_draw(true);
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
        const btQuaternion quaterninon(transform_component->rotation[0], transform_component->rotation[1], transform_component->rotation[2], transform_component->rotation[3]);
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
            transform.rotation[0] = euler_rotation.getX();
            transform.rotation[1] = euler_rotation.getY();
            transform.rotation[2] = euler_rotation.getZ();
            transform.rotation[3] = euler_rotation.getW();
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
    std::span<const Geometry> geometries, std::span<const AnimationClip> animations, std::span<const Skin> skins, std::span<const engine_material_create_desc_t> materials)
{
#if 1
    auto transform_view = entity_registry_.view<engine_tranform_component_t>();
    transform_view.each([this](engine_tranform_component_t& transform_component)
        {
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

    //ToDo: this coule be optimized if entityies are sorted, so parents are always computed first
    auto parent_to_child_transform_view = entity_registry_.view<engine_tranform_component_t, const engine_parent_component_t>();
    parent_to_child_transform_view.each([this](engine_tranform_component_t& transform_comp, const engine_parent_component_t& parent_comp)
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
            std::memcpy(transform_comp.local_to_world, &ltw_matrix, sizeof(ltw_matrix));
        });

    // attach or deattach entity to the skin object
    for(const auto entt : mesh_update_observer)
    {
        const auto mesh_component = get_component<engine_mesh_component_t>(entt);
        auto skin_component = get_component<engine_skin_internal_component_t>(entt);
        if (mesh_component->skin == ENGINE_INVALID_OBJECT_HANDLE)
        {
            skin_component->bone_trs.clear();
            skin_component->bone_animation_transform.clear();
        }
        else
        {
            const auto& skin = skins[mesh_component->skin];
            *skin_component = skin.initalize_skin_component();
        }
    }
    mesh_update_observer.clear();

    auto animation_view = entity_registry_.view<engine_tranform_component_t, engine_skin_internal_component_t, const engine_mesh_component_t, engine_animation_component_t>();
    animation_view.each([&dt, &animations, &skins, this](engine_tranform_component_t&, engine_skin_internal_component_t& skin, const engine_mesh_component_t& mesh, engine_animation_component_t& animation)
        {
            //for (auto i = 0; i < ENGINE_ANIMATIONS_CLIPS_MAX_COUNT; i++)
            for (auto i = 0; i < 1; i++)
            {
                if (animation.animations_state[i] == ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING)
                {
                    continue;
                }

                const auto& animation_data = animations[animation.animations_array[i]];
                auto& animation_dt = animation.animations_dt[i];
                animation_dt += dt;

                // if no skin -> move whole object
                if (mesh.skin == ENGINE_INVALID_OBJECT_HANDLE)
                {
                    //auto ltw = glm::make_mat4(transform.local_to_world);
                    //std::array<glm::mat4, 1> animation_matrix;
                    //if (animation_data.compute_animation_model_matrix(animation_matrix, animation_dt))
                    //{
                    //    ltw *= animation_matrix[0];
                    //    patch_component<engine_tranform_component_t>(entity, [&skin, &ltw](engine_tranform_component_t& c)
                    //        {
                    //            std::memcpy(c.local_to_world, &ltw, sizeof(ltw));
                    //        });
                    //}
                }
                else
                {
                    animation_data.compute_animation_model_matrix(skin.bone_trs, animation_dt);
                }

                if (animation_dt >= animation_data.get_duration())
                {
                    animation.animations_state[i] = ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING;
                    animation_dt = 0.0f;
                }
            }
        }
    );

    auto skinning_view = entity_registry_.view<engine_skin_internal_component_t, const engine_mesh_component_t>();
    skinning_view.each([&skins](engine_skin_internal_component_t& skin, const engine_mesh_component_t& mesh)
        {
            //ToDo: if there is a lot of meshes with and without skin than this if statement is going to be performance costly
            if (mesh.skin != ENGINE_INVALID_OBJECT_HANDLE)
            {
                skin.bone_animation_transform = skins[mesh.skin].compute_transform(skin.bone_trs);
            }
        });

    auto geometry_renderet = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t, const engine_skin_internal_component_t>();
    auto camera_view = entity_registry_.view<const engine_camera_component_t, const engine_tranform_component_t>();

    for (auto [entity, camera, transform] : camera_view.each()) 
    {
        if (!camera.enabled)
        {
            continue;
        }

        const auto window_size_pixels = rdx.get_window_size_in_pixels();

        glm::mat4 view = glm::mat4(0.0);
        glm::mat4 projection = glm::mat4(0.0);
        // update camera: view and projection
        {
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
            const auto eye_position = glm::make_vec3(transform.position);
            const auto up = glm::make_vec3(camera.direction.up);
            const auto target = glm::make_vec3(camera.target);
            view = glm::lookAt(eye_position, target, up);
        }

        //rdx.set_polygon_mode(RenderContext::PolygonFaceType::eFrontAndBack, RenderContext::PolygonMode::eLine);
        geometry_renderet.each([this, &view, &projection, &rdx, &textures, &geometries, &materials](const engine_tranform_component_t& transform, const engine_mesh_component_t& mesh, const engine_material_component_t& material_component, const engine_skin_internal_component_t& skin)
            {
                if (mesh.disable)
                {
                    return;
                }
                
                /*
                * This is not perfect. Probably we want (for optimization purposes) sort meshes by shader used to have too many shader switches and rebining the same data over and over (i.e. view and projection).
                */
                auto bind_and_set_common_variables = [&](Shader& shader, const engine_material_create_desc_t& material)
                {
                    shader.bind();
                    shader.set_uniform_mat_f4("view", { glm::value_ptr(view), sizeof(view) / sizeof(float) });
                    shader.set_uniform_mat_f4("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });
                    shader.set_uniform_f4("diffuse_color", material.diffuse_color);
                    shader.set_uniform_mat_f4("model", transform.local_to_world);

                    const auto texture_diffuse_idx = material.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material.diffuse_texture;
                    shader.set_texture("texture_diffuse", &textures[texture_diffuse_idx]);
                };  

                const auto& material = materials[material_component.material == ENGINE_INVALID_OBJECT_HANDLE ? 0 : material_component.material];
                if (mesh.skin == ENGINE_INVALID_OBJECT_HANDLE)
                {
                    bind_and_set_common_variables(shader_simple_, material);
                }
                else
                {
                    bind_and_set_common_variables(shader_vertex_skinning_, material);

                    const auto& per_bone_animation_data = skin.bone_animation_transform;
                    assert(per_bone_animation_data.size() < 64); // MAX_BONES = 64 in shader!
                    for (std::size_t i = 0; i < per_bone_animation_data.size(); i++)
                    {
                        const auto uniform_name = "global_bone_transform[" + std::to_string(i) + "]";
                        shader_vertex_skinning_.set_uniform_mat_f4(uniform_name, { glm::value_ptr(per_bone_animation_data[i]), sizeof(per_bone_animation_data[i]) / sizeof(float)});
                    }
                }

                geometries[mesh.geometry].bind();
                geometries[mesh.geometry].draw(Geometry::Mode::eTriangles);

			}
		);
        shader_simple_.bind();
        shader_simple_.set_uniform_mat_f4("view", { glm::value_ptr(view), sizeof(view) / sizeof(float) });
        shader_simple_.set_uniform_mat_f4("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });
        shader_simple_.set_uniform_f4("diffuse_color", std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
        glm::mat4x4 amat(1.0f);
        shader_simple_.set_uniform_mat_f4("model", { glm::value_ptr(amat), sizeof(amat) / sizeof(float) });
        shader_simple_.set_texture("texture_diffuse", &textures[0]);

        physics_world_.debug_draw(&rdx,
            { glm::value_ptr(view), sizeof(view) / sizeof(float) },
            { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });

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

