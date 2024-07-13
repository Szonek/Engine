#include "components_initializers.h"
#include "engine.h"

namespace
{
template<typename T>
T& get_zero_init_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = registry.get<T>(entity);
    std::memset(&comp, 0, sizeof(T));
    return comp;
}
} // namespace anonymous

void engine::initialize_transform_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_tranform_component_t>(registry, entity);
    comp.scale[0] = 1.0f;
    comp.scale[1] = 1.0f;
    comp.scale[2] = 1.0f;
    comp.rotation[3] = 1.0f;
}

void engine::initialize_mesh_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_mesh_component_t>(registry, entity);
    comp.geometry = ENGINE_INVALID_OBJECT_HANDLE;
}

void engine::initialize_light_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_light_component_t>(registry, entity);
    comp.type = ENGINE_LIGHT_TYPE_POINT;
    // base point light params
    comp.point.constant = 1.0f;
    comp.point.linear = 0.09f;
    comp.point.quadratic = 0.032f;
    // base intensity
    comp.intensity.ambient[0] = 0.1f;
    comp.intensity.ambient[1] = 0.1f;
    comp.intensity.ambient[2] = 0.1f;
    comp.intensity.diffuse[0] = 1.0f;
    comp.intensity.diffuse[1] = 1.0f;
    comp.intensity.diffuse[2] = 1.0f;
    comp.intensity.specular[0] = 1.0f;
    comp.intensity.specular[1] = 1.0f;
    comp.intensity.specular[2] = 1.0f;

}

void engine::initialize_sprite_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_sprite_component_t>(registry, entity);
}

void engine::initialize_text_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_text_component_t>(registry, entity);
    comp.font_handle = ENGINE_INVALID_OBJECT_HANDLE;
}


void engine::initialize_material_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_material_component_t>(registry, entity);
    comp.type = ENGINE_MATERIAL_TYPE_PONG;
    for (auto i = 0; i < std::size(comp.data.pong.diffuse_color); i++)
    {
        comp.data.pong.diffuse_color[i] = 0.0f;
    }
    comp.data.pong.diffuse_texture = ENGINE_INVALID_OBJECT_HANDLE;
    comp.data.pong.specular_texture = ENGINE_INVALID_OBJECT_HANDLE;
}

void engine::initialize_parent_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_parent_component_t>(registry, entity);
    comp.parent = ENGINE_INVALID_GAME_OBJECT_ID;
}

void engine::initialize_name_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_name_component_t>(registry, entity);
}

void engine::initialize_camera_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_camera_component_t>(registry, entity);
    // disabled by default?
    comp.enabled = 0;
    // clip
    comp.clip_plane_near = 0.1f;
    comp.clip_plane_far = 256.0f;
    // perspective
    comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
    comp.type_union.perspective_fov = 45.0f;
    // default viewport
    comp.viewport_rect.x = 0.0f;
    comp.viewport_rect.y = 0.0f;
    comp.viewport_rect.width = 1.0f;
    comp.viewport_rect.height = 1.0f;
    // direction
    comp.direction.up[0] = 0.0f;
    comp.direction.up[1] = 1.0f;
    comp.direction.up[2] = 0.0f;
}

void engine::initialize_rigidbody_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_rigid_body_component_t>(registry, entity);
}

void engine::initialize_collider_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_collider_component_t>(registry, entity);
}

void engine::initialize_skin_component(entt::registry& registry, entt::entity entity)
{
    auto& comp = get_zero_init_component<engine_skin_component_t>(registry, entity);
    static_assert(ENGINE_INVALID_GAME_OBJECT_ID == 0, "Invalid game object id should be 0. If it's not 0 than update this function to initalize skeleton array.");
    for (auto& bone : comp.bones)
    {
        bone = ENGINE_INVALID_GAME_OBJECT_ID;
    }
}
