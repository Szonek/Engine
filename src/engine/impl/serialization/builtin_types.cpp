#include "builtin_types.h"

#include <engine.h>

// Not described yet, so not saved: the skin, skinned mesh, joint attachment and animation controller components
// (they come with saved prefab instances) and the sprite (a placeholder).

namespace
{
// the cases of the camera's projection union: each is one float
struct OrthographicProjection
{
    float scale;
};
struct PerspectiveProjection
{
    float fov;
};
static_assert(sizeof(OrthographicProjection) == sizeof(engine_camera_component_t::type_union));
static_assert(sizeof(PerspectiveProjection) == sizeof(engine_camera_component_t::type_union));

void register_camera(engine::TypeRegistry& registry)
{
    using namespace engine;
    TypeBuilder<OrthographicProjection>(registry, "orthographic_projection")
        .field("scale", &OrthographicProjection::scale)
        .structure();
    TypeBuilder<PerspectiveProjection>(registry, "perspective_projection")
        .field("fov", &PerspectiveProjection::fov)
        .structure();
    TypeBuilder<engine_viewport_rect_component_t>(registry, "viewport_rect")
        .field("x", &engine_viewport_rect_component_t::x)
        .field("y", &engine_viewport_rect_component_t::y)
        .field("width", &engine_viewport_rect_component_t::width)
        .field("height", &engine_viewport_rect_component_t::height)
        .structure();
    TypeBuilder<engine_camera_direction_t>(registry, "camera_direction")
        .field("up", &engine_camera_direction_t::up)
        .field("right", &engine_camera_direction_t::right)
        .field("front", &engine_camera_direction_t::front)
        .structure();

    using T = engine_camera_component_t;
    TypeBuilder<T>(registry, "camera")
        .field("enabled", &T::enabled)
        .enumeration("type", &T::type, {
            { "orthographic", ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC },
            { "perspective", ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE } })
        .tagged_union("projection", &T::type_union, "type", {
            { ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC, "orthographic_projection" },
            { ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE, "perspective_projection" } })
        .field("target", &T::target)
        .structure("viewport_rect", &T::viewport_rect, "viewport_rect")
        .structure("direction", &T::direction, "camera_direction")
        .field("pitch", &T::pitch)
        .field("yaw", &T::yaw)
        .field("roll", &T::roll)
        .field("clip_plane_near", &T::clip_plane_near)
        .field("clip_plane_far", &T::clip_plane_far)
        .component();
}

void register_light(engine::TypeRegistry& registry)
{
    using namespace engine;
    TypeBuilder<engine_light_intensity_t>(registry, "light_intensity")
        .field("ambient", &engine_light_intensity_t::ambient)
        .field("diffuse", &engine_light_intensity_t::diffuse)
        .field("specular", &engine_light_intensity_t::specular)
        .structure();
    TypeBuilder<engine_directional_light_data_t>(registry, "directional_light")
        .field("direction", &engine_directional_light_data_t::direction)
        .structure();
    TypeBuilder<engine_point_light_data_t>(registry, "point_light")
        .field("constant", &engine_point_light_data_t::constant)
        .field("linear", &engine_point_light_data_t::linear)
        .field("quadratic", &engine_point_light_data_t::quadratic)
        .structure();
    TypeBuilder<engine_spot_light_data_t>(registry, "spot_light")
        .field("direction", &engine_spot_light_data_t::direction)
        .field("cut_off", &engine_spot_light_data_t::cut_off)
        .field("outer_cut_off", &engine_spot_light_data_t::outer_cut_off)
        .field("constant", &engine_spot_light_data_t::constant)
        .field("linear", &engine_spot_light_data_t::linear)
        .field("quadratic", &engine_spot_light_data_t::quadratic)
        .structure();

    using T = engine_light_component_t;
    TypeBuilder<T>(registry, "light")
        .enumeration("type", &T::type, {
            { "directional", ENGINE_LIGHT_TYPE_DIRECTIONAL },
            { "point", ENGINE_LIGHT_TYPE_POINT },
            { "spot", ENGINE_LIGHT_TYPE_SPOT } })
        .structure("intensity", &T::intensity, "light_intensity")
        // the union has no name: its cases are members of the light
        .tagged_union("data", member_offset(&T::directional), "type", {
            { ENGINE_LIGHT_TYPE_DIRECTIONAL, "directional_light" },
            { ENGINE_LIGHT_TYPE_POINT, "point_light" },
            { ENGINE_LIGHT_TYPE_SPOT, "spot_light" } })
        .component();
}

void register_collider(engine::TypeRegistry& registry)
{
    using namespace engine;
    const std::vector<EnumValue> collider_types = {
        { "box", ENGINE_COLLIDER_TYPE_BOX },
        { "sphere", ENGINE_COLLIDER_TYPE_SPHERE },
        { "compound", ENGINE_COLLIDER_TYPE_COMPOUND } };

    TypeBuilder<engine_box_collider_t>(registry, "box_collider")
        .field("size", &engine_box_collider_t::size)
        .structure();
    TypeBuilder<engine_sphere_collider_t>(registry, "sphere_collider")
        .field("radius", &engine_sphere_collider_t::radius)
        .structure();
    TypeBuilder<engine_child_collider_t>(registry, "child_collider")
        .enumeration("type", &engine_child_collider_t::type, collider_types)
        .field("transform", &engine_child_collider_t::transform)
        .field("rotation_quaternion", &engine_child_collider_t::rotation_quaternion)
        // a child is a box or a sphere
        .tagged_union("collider", &engine_child_collider_t::collider, "type", {
            { ENGINE_COLLIDER_TYPE_BOX, "box_collider" },
            { ENGINE_COLLIDER_TYPE_SPHERE, "sphere_collider" } })
        .structure();
    TypeBuilder<engine_compound_collider_t>(registry, "compound_collider")
        .structure("children", &engine_compound_collider_t::children, "child_collider")
        .structure();

    using T = engine_collider_component_t;
    TypeBuilder<T>(registry, "collider")
        .enumeration("type", &T::type, collider_types)
        .field("is_trigger", &T::is_trigger)
        .field("disable", &T::disable)
        .tagged_union("collider", &T::collider, "type", {
            { ENGINE_COLLIDER_TYPE_BOX, "box_collider" },
            { ENGINE_COLLIDER_TYPE_SPHERE, "sphere_collider" },
            { ENGINE_COLLIDER_TYPE_COMPOUND, "compound_collider" } })
        .field("bounciness", &T::bounciness)
        .field("friction_static", &T::friction_static)
        .component();
}
}  // namespace

void engine::register_builtin_types(TypeRegistry& registry)
{
    // what organizes the scene, then the transforms and the hierarchy: the systems reacting to the components after
    // them (bodies, nav mesh build) read world matrices
    TypeBuilder<engine_name_component_t>(registry, "name")
        .field("name", &engine_name_component_t::name)
        .component();
    TypeBuilder<engine_folder_component_t>(registry, "folder")
        .field("path", &engine_folder_component_t::path)
        .component();
    TypeBuilder<engine_transform_component_t>(registry, "transform")
        .field("position", &engine_transform_component_t::position)
        .field("scale", &engine_transform_component_t::scale)
        .field("rotation", &engine_transform_component_t::rotation)
        .component();
    TypeBuilder<engine_parent_component_t>(registry, "parent")
        .game_object("parent", &engine_parent_component_t::parent)
        .component();

    // rendering
    TypeBuilder<engine_mesh_component_t>(registry, "mesh")
        .asset("geometry", &engine_mesh_component_t::geometry, AssetKind::eGeometry)
        .field("disable", &engine_mesh_component_t::disable)
        .component();
    TypeBuilder<engine_material_component_t>(registry, "material")
        .field("diffuse_color", &engine_material_component_t::diffuse_color)
        .asset("diffuse_texture", &engine_material_component_t::diffuse_texture, AssetKind::eTexture)
        .field("shininess", &engine_material_component_t::shininess)
        .asset("specular_texture", &engine_material_component_t::specular_texture, AssetKind::eTexture)
        .component();
    register_light(registry);
    register_camera(registry);

    // navigation
    TypeBuilder<engine_nav_surface_component_t>(registry, "nav_surface")
        .field("agent_radius", &engine_nav_surface_component_t::agent_radius)
        .field("agent_height", &engine_nav_surface_component_t::agent_height)
        .field("agent_max_slope", &engine_nav_surface_component_t::agent_max_slope)
        .field("agent_max_climb", &engine_nav_surface_component_t::agent_max_climb)
        .field("cell_size", &engine_nav_surface_component_t::cell_size)
        .field("cell_height", &engine_nav_surface_component_t::cell_height)
        .component();
    TypeBuilder<engine_nav_modifier_component_t>(registry, "nav_modifier")
        .enumeration("type", &engine_nav_modifier_component_t::type, {
            { "walkable", ENGINE_NAV_MODIFIER_TYPE_WALKABLE },
            { "not_walkable", ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE },
            { "ignore", ENGINE_NAV_MODIFIER_TYPE_IGNORE } })
        .component();

    // physics: the rigid body before the collider, so a body is created with its type
    TypeBuilder<engine_rigid_body_component_t>(registry, "rigid_body")
        .enumeration("type", &engine_rigid_body_component_t::type, {
            { "dynamic", ENGINE_RIGID_BODY_TYPE_DYNAMIC },
            { "kinematic", ENGINE_RIGID_BODY_TYPE_KINEMATIC },
            { "static", ENGINE_RIGID_BODY_TYPE_STATIC } })
        .field("mass", &engine_rigid_body_component_t::mass)
        .component();
    register_collider(registry);
    TypeBuilder<engine_character_controller_component_t>(registry, "character_controller")
        .field("radius", &engine_character_controller_component_t::radius)
        .field("height", &engine_character_controller_component_t::height)
        .field("step_height", &engine_character_controller_component_t::step_height)
        .field("max_slope", &engine_character_controller_component_t::max_slope)
        .component();
}
