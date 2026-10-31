#include "application.h"
#include "application_editor.h"
#include "scene.h"
#include "ui_manager.h"
#include "nav_mesh.h"
#include "nav_mesh_debug_draw.h"
#include "world_transform.h"
#include "hierarchy.h"
#include "logger.h"
#include "math_helpers.h"
#include "components_utils/components_initializers.h"
#include "components_internals/camera_internal_component.h"
#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"
#include "components_internals/skin_pose_component.h"
#include "components_internals/animation_controller_internal_component.h"
#include "components_internals/children_internal_component.h"
#include "components_internals/joint_attachment_internal_component.h"
#include "components_internals/nav_mesh_internal_component.h"
#include "profiler.h"
#include "engine_vector_impl_def.h"
#include "skin.h"
#include "animation_controller.h"
#include "shaders_binding_slots.h"
#include "render/renderer.h"

#include <fmt/format.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <SDL3/SDL.h>

#include <RmlUi/Core.h>

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"

#include "imgui/imgui.h"
#include "imguizmo/ImGuizmo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>


// the joint name string is the engine's: the internal component owns it and the component points at it
static inline void construct_joint_attachment(entt::registry& registry, entt::entity entity)
{
    engine::initialize_joint_attachment_component(registry, entity);
    auto& owned = registry.emplace<engine::joint_attachment_internal_component_t>(entity);
    registry.get<engine_joint_attachment_component_t>(entity).joint_name = owned.joint_name.get();
}

// an update carrying another string (or none) keeps the engine's: its value is copied, the caller keeps its string
static inline void update_joint_attachment(entt::registry& registry, entt::entity entity)
{
    auto& comp = registry.get<engine_joint_attachment_component_t>(entity);
    auto* owned = registry.get<engine::joint_attachment_internal_component_t>(entity).joint_name.get();
    if (comp.joint_name != owned)
    {
        if (comp.joint_name)
        {
            owned->str = comp.joint_name->str;
        }
        comp.joint_name = owned;
    }
}

static inline void calculate_camera_view_and_projection(std::size_t view_width, std::size_t view_height, const glm::vec3& eye_position, const engine_camera_component_t& camera, engine::camera_internal_component_t& camera_internal)
{
    // update camera: view and projection
    ENGINE_PROFILE_SECTION_N("camera_update");
    if(!camera_internal.computed_this_frame)
    {
        camera_internal.computed_this_frame = true;
        const auto z_near = camera.clip_plane_near;
        const auto z_far = camera.clip_plane_far;
        // ToD: multi camera - this should use resolution of camera!!!

        const auto adjusted_width = view_width * (camera.viewport_rect.width - camera.viewport_rect.x);
        const auto adjusted_height = view_height * (camera.viewport_rect.height - camera.viewport_rect.y);
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

engine::Scene::Scene(Application* app, Renderer& renderer, const engine_scene_create_desc_t& /*config*/, engine_result_code_t& out_code)
    : app_(app)
    , renderer_(renderer)
    , assets_(*app)
    , entity_ids_(entity_registry_)
    // connects its listeners before the initializers below: EnTT calls them last-connected first, so its listeners run after them
    , physics_(entity_registry_,
        [this](const engine_joint_attachment_component_t& joint_attachment) { return get_joint_matrix(joint_attachment); },
        [this]() { return dynamic_cast<const ApplicationEditor*>(app_) && !app_->is_playing(); })
    , prefabs_(*app, *this, entity_registry_, assets_)
{
    // the first entity EnTT creates is 0: reserve it, so that no game object has the value of ENGINE_INVALID_GAME_OBJECT_ID
    [[maybe_unused]] const auto reserved = entity_registry_.create();
    assert(reserved == RESERVED_ENTITY);
    // basic initalizers
    entity_registry_.on_construct<engine_transform_component_t>().connect<&initialize_transform_component>();
    entity_registry_.on_construct<engine_transform_component_t>().connect<&entt::registry::emplace<world_transform_internal_component_t>>();
    entity_registry_.on_destroy<engine_transform_component_t>().connect<&entt::registry::remove<world_transform_internal_component_t>>();
    entity_registry_.on_construct<engine_mesh_component_t>().connect<&initialize_mesh_component>();
    entity_registry_.on_construct<engine_skinned_mesh_component_t>().connect<&initialize_skinned_mesh_component>();
    entity_registry_.on_construct<engine_material_component_t>().connect<&initialize_material_component>();
    entity_registry_.on_construct<engine_parent_component_t>().connect<&initialize_parent_component>();
    entity_registry_.on_construct<engine_name_component_t>().connect<&initialize_name_component>();
    entity_registry_.on_construct<engine_folder_component_t>().connect<&initialize_folder_component>();
    entity_registry_.on_construct<engine_camera_component_t>().connect<&initialize_camera_component>();
    entity_registry_.on_construct<engine_camera_component_t>().connect<&entt::registry::emplace<camera_internal_component_t>>();
    entity_registry_.on_construct<engine_skin_component_t>().connect<&initialize_skin_component>();
    // only on update: EnTT calls construction listeners last-connected first, so on construction this would run before
    // initialize_skin_component and read a zeroed handle (a valid atlas slot). A new component has no skin yet anyway.
    entity_registry_.on_update<engine_skin_component_t>().connect<&Scene::on_skin_component_changed>(*this);
    entity_registry_.on_destroy<engine_skin_component_t>().connect<&entt::registry::remove<skin_pose_internal_component_t>>();
    // a controller plays on its game object's skin: (re)built when either its animation set or the skin changes
    entity_registry_.on_update<engine_animation_controller_component_t>().connect<&Scene::on_animation_controller_component_changed>(*this);
    entity_registry_.on_destroy<engine_animation_controller_component_t>().connect<&entt::registry::remove<animation_controller_internal_component_t>>();
    entity_registry_.on_destroy<engine_skin_component_t>().connect<&entt::registry::remove<animation_controller_internal_component_t>>();
    entity_registry_.on_construct<engine_joint_attachment_component_t>().connect<&construct_joint_attachment>();
    entity_registry_.on_update<engine_joint_attachment_component_t>().connect<&update_joint_attachment>();
    entity_registry_.on_destroy<engine_joint_attachment_component_t>().connect<&entt::registry::remove<joint_attachment_internal_component_t>>();
    entity_registry_.on_construct<engine_animation_controller_component_t>().connect<&initialize_animation_controller_component>();
    entity_registry_.on_construct<engine_light_component_t>().connect<&initialize_light_component>();
    entity_registry_.on_construct<engine_sprite_component_t>().connect<&initialize_sprite_component>();
    entity_registry_.on_construct<engine_nav_surface_component_t>().connect<&initialize_nav_surface_component>();
    entity_registry_.on_construct<engine_nav_modifier_component_t>().connect<&initialize_nav_modifier_component>();
    entity_registry_.on_destroy<engine_nav_surface_component_t>().connect<&entt::registry::remove<nav_mesh_internal_component_t>>();
    
    connect_hierarchy(entity_registry_);

    out_code = ENGINE_RESULT_CODE_OK;
}

engine::Scene::~Scene()
{  
}

engine::Application* engine::Scene::get_application() const
{
    return app_;
}

engine::PrefabSystem& engine::Scene::prefabs()
{
    return prefabs_;
}

const engine::PrefabSystem& engine::Scene::prefabs() const
{
    return prefabs_;
}

engine::SceneAssets& engine::Scene::assets()
{
    return assets_;
}

engine::PhysicsSystem& engine::Scene::physics()
{
    return physics_;
}

const engine::PhysicsSystem& engine::Scene::physics() const
{
    return physics_;
}

void engine::Scene::enable_nav_mesh_debug_draw(bool enable)
{
    nav_mesh_debug_draw_ = enable;
}

bool engine::Scene::is_nav_mesh_debug_draw_enabled() const
{
    return nav_mesh_debug_draw_;
}

engine_result_code_t engine::Scene::build_nav_mesh()
{
    ENGINE_PROFILE_SECTION;
    auto surfaces = entity_registry_.view<const engine_nav_surface_component_t>();
    if (surfaces.begin() == surfaces.end())
    {
        log::log(log::LogLevel::eError, "Nav mesh build: the scene has no nav surface component.\n");
        return ENGINE_RESULT_CODE_FAIL;
    }
    // one nav surface per scene for now; when there are more (e.g. a duplicated one), the one with the lowest persistent
    // ID, so the choice doesn't depend on the storage's order (transient ones last), and the others keep no nav mesh,
    // so the queries use the same one
    const auto surface_order = [this](entt::entity e)
        {
            const auto id = get_persistent_id(e);
            return id == EntityIdRegistry::INVALID_ID ? std::numeric_limits<std::uint64_t>::max() : id;
        };
    auto surface_entity = *surfaces.begin();
    std::size_t count = 0;
    for (const auto surface : surfaces)
    {
        count++;
        if (surface_order(surface) < surface_order(surface_entity))
        {
            surface_entity = surface;
        }
    }
    if (count > 1)
    {
        log::log(log::LogLevel::eError, fmt::format("Nav mesh build: the scene has {} nav surface components, using the one with the lowest "
            "persistent ID ({}).\n", count, get_persistent_id(surface_entity)));
        for (const auto other : surfaces)
        {
            if (other != surface_entity)
            {
                entity_registry_.remove<nav_mesh_internal_component_t>(other);
            }
        }
    }
    const auto& surface = surfaces.get<const engine_nav_surface_component_t>(surface_entity);
    NavMeshBuildSettings settings{};
    settings.agent_radius = surface.agent_radius;
    settings.agent_height = surface.agent_height;
    settings.agent_max_slope = surface.agent_max_slope;
    settings.agent_max_climb = surface.agent_max_climb;
    settings.cell_size = surface.cell_size;
    settings.cell_height = surface.cell_height;

    // the colliders are placed by their world transforms, which are not computed yet for objects created this frame
    update_world_transforms();
    NavMeshInput input;
    auto colliders = entity_registry_.view<const world_transform_internal_component_t, const engine_collider_component_t>();
    colliders.each([this, &input](auto entity, const world_transform_internal_component_t& world, const engine_collider_component_t& collider)
        {
            // only static colliders: moving bodies (dynamic, kinematic), triggers and disabled colliders don't block for good
            const auto* rigid_body = entity_registry_.try_get<engine_rigid_body_component_t>(entity);
            if (collider.is_trigger || collider.disable || (rigid_body && rigid_body->type == ENGINE_RIGID_BODY_TYPE_KINEMATIC)
                || (rigid_body && rigid_body->type == ENGINE_RIGID_BODY_TYPE_DYNAMIC && rigid_body->mass > 0.0f))
            {
                return;
            }
            auto area = NavArea::eWalkable;
            if (const auto* modifier = find_nav_modifier(entity))
            {
                if (modifier->type == ENGINE_NAV_MODIFIER_TYPE_IGNORE)
                {
                    return;
                }
                if (modifier->type == ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE)
                {
                    area = NavArea::eNotWalkable;
                }
            }
            input.add_collider(collider, world.local_to_world, area);
        });

    auto nav_mesh = NavMesh::build(input, settings);
    if (!nav_mesh)
    {
        entity_registry_.remove<nav_mesh_internal_component_t>(surface_entity);
        return ENGINE_RESULT_CODE_FAIL;
    }
    entity_registry_.emplace_or_replace<nav_mesh_internal_component_t>(surface_entity, std::move(nav_mesh));
    return ENGINE_RESULT_CODE_OK;
}

const engine::NavMesh* engine::Scene::get_nav_mesh() const
{
    const auto nav_meshes = entity_registry_.view<const nav_mesh_internal_component_t>();
    if (nav_meshes.begin() == nav_meshes.end())
    {
        return nullptr;
    }
    return nav_meshes.get<const nav_mesh_internal_component_t>(*nav_meshes.begin()).nav_mesh.get();
}

const engine_nav_modifier_component_t* engine::Scene::find_nav_modifier(entt::entity entity) const
{
    while (entity_registry_.valid(entity))
    {
        if (const auto* modifier = entity_registry_.try_get<engine_nav_modifier_component_t>(entity))
        {
            return modifier;
        }
        const auto* parent = entity_registry_.try_get<engine_parent_component_t>(entity);
        if (!parent || parent->parent == ENGINE_INVALID_GAME_OBJECT_ID)
        {
            return nullptr;
        }
        entity = static_cast<entt::entity>(parent->parent);
    }
    return nullptr;
}

std::optional<glm::mat4> engine::Scene::get_joint_matrix(const engine_joint_attachment_component_t& joint_attachment) const
{
    const auto [pose, skin] = get_skin_instance(joint_attachment.skin);
    if (!pose)
    {
        log::log(log::LogLevel::eError, fmt::format("Joint attachment references game object {}, which has no skin. Are you sure you are doing valid thing?\n", joint_attachment.skin));
        return std::nullopt;
    }
    return skin->get_model_matrix_for_joint(*pose, joint_attachment.joint_name->str);
}

glm::mat4 engine::Scene::get_parent_space_matrix(entt::entity entity) const
{
    return engine::get_parent_space_matrix(entity_registry_, entity, [this](const auto& joint_attachment) { return get_joint_matrix(joint_attachment); });
}

glm::mat4 engine::Scene::compute_world_matrix(entt::entity entity) const
{
    return engine::compute_world_matrix(entity_registry_, entity, [this](const auto& joint_attachment) { return get_joint_matrix(joint_attachment); });
}

void engine::Scene::update_world_transforms()
{
    engine::update_world_transforms(entity_registry_, [this](const auto& joint_attachment) { return get_joint_matrix(joint_attachment); });
}

engine_result_code_t engine::Scene::update(float dt)
{
    ENGINE_PROFILE_SECTION;
    {
        ENGINE_PROFILE_SECTION_N("animation_controller_update_view");
        // a controller drives the pose of the skin instance on its own game object
        auto animation_controller_updated_view = entity_registry_.view<animation_controller_internal_component_t, skin_pose_internal_component_t>();
        animation_controller_updated_view.each([dt](animation_controller_internal_component_t& ac_component, skin_pose_internal_component_t& pose_component)
            {
                ac_component.controller->update(dt, pose_component.pose);
            });
    }

    // physics computes the world transforms of its bodies from the transforms (it can't wait for this pass: it answers
    // queries during the scripts' update); this pass is for everything else, after the step moved the dynamic bodies
    physics_.update(dt);
    update_world_transforms();

    {
        ENGINE_PROFILE_SECTION_N("skin_component_view");
        auto skin_component_view = entity_registry_.view<const engine_skin_component_t, skin_pose_internal_component_t>();
        skin_component_view.each([this](const engine_skin_component_t& skin_component, skin_pose_internal_component_t& pose_component)
            {
                if (const auto* skin = app_->get_skin(skin_component.skin))
                {
                    skin->compute_skinning_matrices(pose_component.pose);
                }
            });
    }

    SceneRenderData render_data;
    std::uint32_t directional_light_count = 0;
    std::uint32_t point_light_count = 0;
    std::uint32_t spot_light_count = 0;
    // gather lights for the renderer
    {
        ENGINE_PROFILE_SECTION_N("lights update");
        auto lights_view = entity_registry_.view<const world_transform_internal_component_t, const engine_light_component_t>();
        {
            ENGINE_PROFILE_SECTION_N("lights counter");
            lights_view.each([&directional_light_count, &point_light_count, &spot_light_count](const world_transform_internal_component_t&, const engine_light_component_t& light)
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
            ENGINE_PROFILE_SECTION_N("lights data");
            const auto total_lights = directional_light_count + point_light_count + spot_light_count;
            render_data.lights.assign(total_lights, LightGpuData{});
            auto* light_gpu_data_ptr = render_data.lights.data();
            std::uint32_t dir_idx = 0;
            std::uint32_t point_idx = directional_light_count;
            std::uint32_t spot_idx = directional_light_count + point_light_count;
            lights_view.each([&light_gpu_data_ptr, &dir_idx, &point_idx, &spot_idx](const world_transform_internal_component_t& world, const engine_light_component_t& light)
                {
                    LightGpuData* light_data_ptr = light_gpu_data_ptr;
                    if (light.type == ENGINE_LIGHT_TYPE_DIRECTIONAL)
                    {
                        light_data_ptr = &light_data_ptr[dir_idx++];
                        light_data_ptr->direction = glm::make_vec3(light.directional.direction);
                    }
                    else if (light.type == ENGINE_LIGHT_TYPE_POINT)
                    {
                        light_data_ptr = &light_data_ptr[point_idx++];
                        light_data_ptr->position = glm::vec3(world.local_to_world[3]);
                        light_data_ptr->constant = light.point.constant;
                        light_data_ptr->linear = light.point.linear;
                        light_data_ptr->quadratic = light.point.quadratic;
                    }
                    else if (light.type == ENGINE_LIGHT_TYPE_SPOT)
                    {
                        light_data_ptr = &light_data_ptr[spot_idx++];
                        light_data_ptr->position = glm::vec3(world.local_to_world[3]);
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

            assert(dir_idx == directional_light_count);
            assert(point_idx == directional_light_count + point_light_count);
            assert(spot_idx == directional_light_count + point_light_count + spot_light_count);
        }

    }

    render_data.scene_gpu.direction_light_count = directional_light_count;
    render_data.scene_gpu.point_light_count = point_light_count;
    render_data.scene_gpu.spot_light_count = spot_light_count;

    submit_render_data(std::move(render_data));
    return ENGINE_RESULT_CODE_OK;
}

void engine::Scene::submit_render_data(SceneRenderData&& render_data)
{
    ENGINE_PROFILE_SECTION;
    {
        ENGINE_PROFILE_SECTION_N("meshes");
        auto mesh_renderer = entity_registry_.view<const world_transform_internal_component_t, const engine_mesh_component_t, const engine_material_component_t>(entt::exclude<engine_skin_component_t>);
        auto skinned_mesh_renderer = entity_registry_.view<const world_transform_internal_component_t, const engine_skinned_mesh_component_t, const engine_material_component_t>();
        auto sprite_renderer = entity_registry_.view<const world_transform_internal_component_t, const engine_material_component_t, const engine_sprite_component_t>();
        auto camera_view = entity_registry_.view<const engine_camera_component_t, const engine_transform_component_t, camera_internal_component_t>();
        const auto scene_viewport = app_->get_scene_viewport();

        camera_view.each([&](auto, const engine_camera_component_t& camera, const engine_transform_component_t& camera_transform, camera_internal_component_t& camera_internal)
        {
            if (!camera.enabled)
            {
                return;
            }

            calculate_camera_view_and_projection(scene_viewport.width, scene_viewport.height,
                glm::make_vec3(camera_transform.position), camera, camera_internal);
            render_data.views.push_back(camera_internal.data);
            // data was taken, so the next frame has to compute it again
            camera_internal.computed_this_frame = false;
        });

        // World position of the topmost ancestor with a transform: the snap origin of everything in its hierarchy.
        const auto hierarchy_root_position = [this](entt::entity entity)
            {
                auto root_transform = entity_registry_.try_get<world_transform_internal_component_t>(entity);
                // depth limit only guards against a broken (cyclic) hierarchy
                for (int depth = 0; depth < 64; depth++)
                {
                    const auto parent_component = entity_registry_.try_get<engine_parent_component_t>(entity);
                    if (!parent_component)
                    {
                        break;
                    }
                    const auto parent = static_cast<entt::entity>(parent_component->parent);
                    if (!entity_registry_.valid(parent))
                    {
                        break;
                    }
                    entity = parent;
                    if (const auto transform = entity_registry_.try_get<world_transform_internal_component_t>(entity))
                    {
                        root_transform = transform;
                    }
                }
                return root_transform ? glm::vec3(root_transform->local_to_world[3]) : glm::vec3(0.0f);
            };

        // Fills data shared by all mesh draws. Returns false if the draw has to be skipped.
        const auto fill_mesh_draw_item = [this, &hierarchy_root_position](entt::entity entity, const world_transform_internal_component_t& world, const engine_material_component_t& material_component, engine_geometry_t geometry, StaticMeshDrawItem& item)
            {
                if (geometry == ENGINE_INVALID_OBJECT_HANDLE)
                {
                    log::log(log::LogLevel::eError, fmt::format("[Entity: {}]. Mesh component has invalid geometry handle. Are you sure you are doing valid thing?\n", (std::uint32_t)entity));
                    return false;
                }

                const auto texture_diffuse_idx = material_component.diffuse_texture == ENGINE_INVALID_OBJECT_HANDLE ? get_application()->get_default_texture() : material_component.diffuse_texture;
                // ToDo: specular texture is not used here yet, diffuse one is used for both (that's how it was always working)
                item.entity_id = static_cast<std::uint32_t>(entity);
                item.geometry = get_application()->get_geometry(geometry);
                item.model = world.local_to_world;
                item.snap_origin = hierarchy_root_position(entity);
                item.diffuse_color = glm::make_vec4(material_component.diffuse_color);
                item.shininess = static_cast<float>(material_component.shininess);
                item.texture_diffuse = get_application()->get_texture(texture_diffuse_idx);
                item.texture_specular = get_application()->get_texture(texture_diffuse_idx);
                // a handle to an asset which was released (e.g. with the prefab of its model) finds nothing
                if (!item.geometry || !item.texture_diffuse)
                {
                    log::log(log::LogLevel::eError, fmt::format("[Entity: {}]. Mesh or material uses a geometry or texture which doesn't exist (any more). Is its prefab still loaded?\n", (std::uint32_t)entity));
                    return false;
                }
                return true;
            };

        mesh_renderer.each([&](auto entity, const world_transform_internal_component_t& world, const engine_mesh_component_t& mesh_component, const engine_material_component_t& material_component)
            {
                if (mesh_component.disable)
                {
                    return;
                }
                StaticMeshDrawItem item;
                if (fill_mesh_draw_item(entity, world, material_component, mesh_component.geometry, item))
                {
                    render_data.static_meshes.push_back(item);
                }
            }
        );

        skinned_mesh_renderer.each([&](auto entity, const world_transform_internal_component_t& world, const engine_skinned_mesh_component_t& skinned_mesh_component, const engine_material_component_t& material_component)
            {
                if (skinned_mesh_component.disable)
                {
                    return;
                }
                const auto [pose, skin] = get_skin_instance(skinned_mesh_component.skin);
                if (!pose)
                {
                    log::log(log::LogLevel::eError, fmt::format("[Entity: {}]. Skinned mesh component has no skin assigned. Are you sure you are doing valid thing?\n", (std::uint32_t)entity));
                    return;
                }
                SkinnedMeshDrawItem item;
                if (!fill_mesh_draw_item(entity, world, material_component, skinned_mesh_component.geometry, item))
                {
                    return;
                }
                const auto& bones = pose->skinning_matrices;
                item.skinning_matrices_offset = static_cast<std::uint32_t>(render_data.skinning_matrices.size());
                item.skinning_matrices_count = static_cast<std::uint32_t>(bones.size());
                render_data.skinning_matrices.insert(render_data.skinning_matrices.end(), bones.begin(), bones.end());
                render_data.skinned_meshes.push_back(item);
            }
        );

        sprite_renderer.each([&](const world_transform_internal_component_t& world, const engine_material_component_t& /*material_component*/, const engine_sprite_component_t& /*sprite_component*/)
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                const auto res = glm::decompose(world.local_to_world, scale, rotation, translation, skew, perspective);
                assert(res);
                render_data.sprites.push_back(SpriteDrawItem{ .world_position = translation, .scale = scale });
            }
        );

        auto outline_view = entity_registry_.view<const outline_component_t, const world_transform_internal_component_t, const engine_mesh_component_t, const engine_material_component_t>();
        outline_view.each([&](auto entity, const outline_component_t&, const world_transform_internal_component_t& world, const engine_mesh_component_t& mesh_component, const engine_material_component_t&)
            {
                const auto* geometry = get_application()->get_geometry(mesh_component.geometry);
                if (!geometry)
                {
                    return;
                }
                render_data.outline_items.push_back(OutlineDrawItem{
                    .geometry = geometry,
                    .model = world.local_to_world,
                    .snap_origin = hierarchy_root_position(entity) });
            }
        );
    }

    if (physics_.is_debug_draw_enabled())
    {
        physics_.collect_debug_lines(render_data.debug_lines);
    }
    if (nav_mesh_debug_draw_)
    {
        if (const auto* nav_mesh = get_nav_mesh())
        {
            collect_nav_mesh_debug_lines(*nav_mesh, render_data.debug_lines);
        }
    }

    renderer_.submit_scene(std::move(render_data));
}

void engine::Scene::on_skin_component_changed(entt::registry& registry, entt::entity entity)
{
    const auto* skin = app_->get_skin(registry.get<engine_skin_component_t>(entity).skin);
    if (!skin)
    {
        registry.remove<skin_pose_internal_component_t, animation_controller_internal_component_t>(entity);
        return;
    }
    registry.emplace_or_replace<skin_pose_internal_component_t>(entity, skin_pose_internal_component_t{ skin->make_rest_pose() });
    // a controller built for another skin is rebuilt for this one
    const auto* controller = registry.try_get<animation_controller_internal_component_t>(entity);
    if (registry.all_of<engine_animation_controller_component_t>(entity) && (!controller || controller->controller->get_skin() != skin))
    {
        on_animation_controller_component_changed(registry, entity);
    }
}

void engine::Scene::on_animation_controller_component_changed(entt::registry& registry, entt::entity entity)
{
    const auto& component = registry.get<engine_animation_controller_component_t>(entity);
    const auto animation_set_handle = component.animations;
    const auto* skin_component = registry.try_get<engine_skin_component_t>(entity);
    const auto* skin = skin_component ? app_->get_skin(skin_component->skin) : nullptr;

    // same animation set and skin: only the layer setup changed, keep the controller (and its animation events)
    if (auto* existing = registry.try_get<animation_controller_internal_component_t>(entity);
        existing && skin && existing->animation_set == animation_set_handle && existing->controller->get_skin() == skin)
    {
        if (!existing->controller->configure_layers(component))
        {
            log::log(log::LogLevel::eError, fmt::format("[Entity: {}] Some animation layers couldn't be set up as described.\n", static_cast<std::uint32_t>(entity)));
        }
        return;
    }

    registry.remove<animation_controller_internal_component_t>(entity);
    if (animation_set_handle == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return;
    }
    auto animation_set = app_->get_animation_set(animation_set_handle);
    if (!skin || !animation_set)
    {
        log::log(log::LogLevel::eError, fmt::format("[Entity: {}] An animation controller needs a valid animation set and a skin on the same game object.\n", static_cast<std::uint32_t>(entity)));
        return;
    }
    if (!animation_set->is_compatible_with(*skin))
    {
        log::log(log::LogLevel::eError, fmt::format("[Entity: {}] The animation set {} was built for another skeleton.\n", static_cast<std::uint32_t>(entity), app_->get_animation_set_name(animation_set_handle)));
        return;
    }
    auto controller = std::make_unique<AnimationController>(skin, std::move(animation_set));
    if (!controller->configure_layers(component))
    {
        log::log(log::LogLevel::eError, fmt::format("[Entity: {}] Some animation layers couldn't be set up as described.\n", static_cast<std::uint32_t>(entity)));
    }
    registry.emplace<animation_controller_internal_component_t>(entity, std::move(controller), animation_set_handle);
}

std::span<const entt::entity> engine::Scene::get_children(entt::entity entity) const
{
    const auto* children = entity_registry_.try_get<children_internal_component_t>(entity);
    return children ? std::span<const entt::entity>(children->children) : std::span<const entt::entity>();
}

std::vector<entt::entity> engine::Scene::get_subtree(entt::entity entity) const
{
    std::vector<entt::entity> subtree{ entity };
    for (std::size_t i = 0; i < subtree.size(); i++)
    {
        const auto children = get_children(subtree[i]);
        subtree.insert(subtree.end(), children.begin(), children.end());
    }
    return subtree;
}

void engine::Scene::set_child_index(entt::entity parent, entt::entity child, std::size_t index)
{
    auto* children = entity_registry_.try_get<children_internal_component_t>(parent);
    if (!children)
    {
        return;
    }
    auto& list = children->children;
    const auto it = std::find(list.begin(), list.end(), child);
    if (it == list.end())
    {
        return;
    }
    list.erase(it);
    list.insert(list.begin() + static_cast<std::ptrdiff_t>(std::min(index, list.size())), child);
}

engine::AnimationController* engine::Scene::get_animation_controller(entt::entity entity)
{
    if (!entity_registry_.valid(entity))
    {
        return nullptr;
    }
    auto* component = entity_registry_.try_get<animation_controller_internal_component_t>(entity);
    return component ? component->controller.get() : nullptr;
}

std::pair<const engine::SkinPose*, const engine::Skin*> engine::Scene::get_skin_instance(engine_game_object_t skin_owner) const
{
    const auto entity = static_cast<entt::entity>(skin_owner);
    if (skin_owner == ENGINE_INVALID_GAME_OBJECT_ID || !entity_registry_.valid(entity))
    {
        return { nullptr, nullptr };
    }
    const auto* pose_component = entity_registry_.try_get<skin_pose_internal_component_t>(entity);
    const auto* skin_component = entity_registry_.try_get<engine_skin_component_t>(entity);
    const auto* skin = skin_component ? app_->get_skin(skin_component->skin) : nullptr;
    // never a pose without the skin it belongs to
    if (!pose_component || !skin)
    {
        return { nullptr, nullptr };
    }
    return { &pose_component->pose, skin };
}

entt::entity engine::Scene::create_new_entity()
{
    ENGINE_PROFILE_SECTION;
    const auto entity = entity_registry_.create();
    entity_ids_.assign_next(entity);
    return entity;
}

entt::entity engine::Scene::create_transient_entity()
{
    ENGINE_PROFILE_SECTION;
    return entity_registry_.create();
}

entt::entity engine::Scene::create_prefab_node_entity(std::uint64_t root_id, std::string_view node_path, const EntityHandles* node_handles)
{
    ENGINE_PROFILE_SECTION;
    const auto id = EntityIdRegistry::hashed_id(root_id, node_path);
    const auto entity = entity_ids_.create_hashed(id, node_handles);
    if (const auto new_id = entity_ids_.get_id(entity); new_id != id)
    {
        log::log(log::LogLevel::eError, fmt::format("Scene: node {} of the prefab instance {} gets the ID {}, its hashed ID {} is used already.\n",
            node_path, root_id, new_id, id));
    }
    return entity;
}

void engine::Scene::destroy_entity(entt::entity entity)
{
    ENGINE_PROFILE_SECTION;
    if (!is_valid_entity(entity))
    {
        return;
    }
    // a copy: destroying a child removes it from the list
    const auto children_view = get_children(entity);
    const std::vector<entt::entity> children(children_view.begin(), children_view.end());
    for (const auto child : children)
    {
        destroy_entity(child);
    }
    entity_registry_.destroy(entity);
}

bool engine::Scene::is_valid_entity(entt::entity entity) const
{
    ENGINE_PROFILE_SECTION;
    return entity != RESERVED_ENTITY && entity_registry_.valid(entity);
}

std::uint64_t engine::Scene::get_persistent_id(entt::entity entity) const
{
    return entity_ids_.get_id(entity);
}

entt::entity engine::Scene::find_entity(std::uint64_t persistent_id) const
{
    return entity_ids_.find(persistent_id);
}

entt::runtime_view engine::Scene::create_runtime_view()
{
    ENGINE_PROFILE_SECTION;
    return entt::runtime_view{};
}

std::string engine::Scene::save() const
{
    ENGINE_PROFILE_SECTION;
    return save_scene(app_->get_type_registry(), entity_registry_, entity_ids_, assets_, prefabs_);
}

std::string engine::Scene::save(std::span<const entt::entity> entities) const
{
    ENGINE_PROFILE_SECTION;
    return save_game_objects(app_->get_type_registry(), entity_registry_, entity_ids_, assets_, prefabs_, entities);
}

bool engine::Scene::load(std::string_view json, std::string_view file_name, const SceneLoadOptions& options)
{
    ENGINE_PROFILE_SECTION;
    // a scene prefab placed in the file (or deeper) which is this file is placed inside itself: it isn't
    prefabs_.begin_file_load(file_name);
    const auto loaded = load_scene(json, file_name, app_->get_type_registry(), entity_registry_, entity_ids_, assets_, prefabs_, options);
    prefabs_.end_file_load();
    return loaded;
}

bool engine::Scene::replace(std::string_view json, std::string_view file_name)
{
    ENGINE_PROFILE_SECTION;
    // nothing is cleared when the file can't be loaded
    if (!check_scene_file(json, file_name))
    {
        return false;
    }
    // the old content's files, held until the new content holds the ones it uses too
    SceneAssets old_assets(*app_);
    old_assets.swap(assets_);
    clear_game_objects(entity_registry_);
    return load(json, file_name);
}

std::vector<entt::entity> engine::Scene::get_all_entities() const
{
    ENGINE_PROFILE_SECTION;
    std::vector<entt::entity> entities;
    for (const auto entity : entity_registry_.view<entt::entity>())
    {
        if (entity != RESERVED_ENTITY)
        {
            entities.push_back(entity);
        }
    }
    return entities;
}

glm::vec3 engine::Scene::convert_world_point_to_screen_point(const glm::vec3& world_point, engine_game_object_t camera_go)
{
    ENGINE_PROFILE_SECTION;
    auto ret = glm::vec3(0.5f, 0.5f, 0.0f);
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(entt::entity(camera_go));
    const auto camera_transform = entity_registry_.try_get<engine_transform_component_t>(entt::entity(camera_go));
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(entt::entity(camera_go));
        const auto scene_viewport = app_->get_scene_viewport();
        calculate_camera_view_and_projection(scene_viewport.width, scene_viewport.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);
        ret = glm::project(world_point, camera_internal_component.data.view, camera_internal_component.data.projection, glm::vec4(0, 0, scene_viewport.width, scene_viewport.height));
        ret.x /= scene_viewport.width;
        ret.y /= scene_viewport.height;
    }
    return ret;
}

glm::vec3 engine::Scene::convert_screen_point_to_world_point(glm::vec3 screen_point, engine_game_object_t camera_go)
{
    ENGINE_PROFILE_SECTION;
    auto ret = glm::vec3(0.0f, 0.0f, 0.0f);
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(entt::entity(camera_go));
    const auto camera_transform = entity_registry_.try_get<engine_transform_component_t>(entt::entity(camera_go));
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(entt::entity(camera_go));    
        const auto scene_viewport = app_->get_scene_viewport();
        calculate_camera_view_and_projection(scene_viewport.width, scene_viewport.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);

        screen_point.x *= scene_viewport.width;
        screen_point.y *= scene_viewport.height;
        ret = glm::unProject(screen_point, camera_internal_component.data.view, camera_internal_component.data.projection, glm::vec4(0, 0, scene_viewport.width, scene_viewport.height));
    }
    return ret;
}

glm::mat4 engine::Scene::get_camera_view(entt::entity camera_go)
{
    ENGINE_PROFILE_SECTION;
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(camera_go);
    const auto camera_transform = entity_registry_.try_get<engine_transform_component_t>(camera_go);
    const auto scene_viewport = app_->get_scene_viewport();
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(camera_go);
        calculate_camera_view_and_projection(scene_viewport.width, scene_viewport.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);
        return camera_internal_component.data.view;
    }
    return glm::mat4();
}

glm::mat4 engine::Scene::get_camera_projection(entt::entity camera_go)
{
    ENGINE_PROFILE_SECTION;
    const auto camera_component = entity_registry_.try_get<engine_camera_component_t>(camera_go);
    const auto camera_transform = entity_registry_.try_get<engine_transform_component_t>(camera_go);
    const auto scene_viewport = app_->get_scene_viewport();
    if (camera_component && camera_transform)
    {
        auto& camera_internal_component = entity_registry_.get<camera_internal_component_t>(camera_go);
        calculate_camera_view_and_projection(scene_viewport.width, scene_viewport.height,
            glm::make_vec3(camera_transform->position), *camera_component, camera_internal_component);
        return camera_internal_component.data.projection;
    }
    return glm::mat4();
}
