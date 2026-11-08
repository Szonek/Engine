#include "builtin_types.h"
#include "../particles.h"

#include <engine.h>

// Not described, so not saved: the sprite (a placeholder without data).

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

// The skin and what reads its pose: the joint attachment and the animation controller (the skinned mesh is with the
// rendering). Each references the game object with the skin component, whose pose it reads, not the skin asset.
void register_skeletal_animation(engine::TypeRegistry& registry)
{
    using namespace engine;
    TypeBuilder<engine_skin_component_t>(registry, "skin")
        .asset("skin", &engine_skin_component_t::skin, AssetKind::eSkin)
        .component();
    TypeBuilder<engine_joint_attachment_component_t>(registry, "joint_attachment")
        .game_object("skin", &engine_joint_attachment_component_t::skin)
        .engine_string("joint_name", &engine_joint_attachment_component_t::joint_name)
        .component();

    TypeBuilder<engine_animation_layer_joint_weight_t>(registry, "animation_layer_joint_weight")
        .field("joint_name", &engine_animation_layer_joint_weight_t::joint_name)
        .field("weight", &engine_animation_layer_joint_weight_t::weight)
        .structure();
    TypeBuilder<engine_animation_layer_desc_t>(registry, "animation_layer")
        .field("name", &engine_animation_layer_desc_t::name)
        .enumeration("mode", &engine_animation_layer_desc_t::mode, {
            { "override", ENGINE_ANIMATION_LAYER_MODE_OVERRIDE },
            { "additive", ENGINE_ANIMATION_LAYER_MODE_ADDITIVE } })
        .field("weight", &engine_animation_layer_desc_t::weight)
        .field("looping", &engine_animation_layer_desc_t::looping)
        .field("joint_weights_count", &engine_animation_layer_desc_t::joint_weights_count)
        .structure("joint_weights", &engine_animation_layer_desc_t::joint_weights, "animation_layer_joint_weight")
        .structure();
    // the layers are a fixed array with a count, like the compound collider's children: every element is written
    using T = engine_animation_controller_component_t;
    TypeBuilder<T>(registry, "animation_controller")
        .asset("animations", &T::animations, AssetKind::eAnimationSet)
        .field("layers_count", &T::layers_count)
        .structure("layers", &T::layers, "animation_layer")
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
        .field("layer", &T::layer)
        .field("collides_with", &T::collides_with)
        .component();
}
engine::FieldHints tip(std::string_view tooltip)
{
    engine::FieldHints h{};
    h.tooltip = std::string(tooltip);
    return h;
}

engine::FieldHints ranged(float min, float max, std::string_view tooltip)
{
    auto h = tip(tooltip);
    h.range = std::make_pair(min, max);
    return h;
}

engine::FieldHints color(std::string_view tooltip)
{
    auto h = tip(tooltip);
    h.color = true;
    return h;
}

// the particle effect (ParticleEffectDesc), a data asset type: files under assets/data/effects
void register_particle_effect(engine::TypeRegistry& registry)
{
    using namespace engine;
    TypeBuilder<ParticleRange>(registry, "particle_range")
        .field("min", &ParticleRange::min)
        .field("max", &ParticleRange::max)
        .structure();
    TypeBuilder<ParticleBurst>(registry, "particle_burst")
        .field("time", &ParticleBurst::time).hints(ranged(0.0f, 600.0f, "seconds into the emission (bursts in time order)"))
        .field("count", &ParticleBurst::count).hints(ranged(0.0f, 10000.0f, "particles it spawns at once"))
        .structure();
    TypeBuilder<ParticleCurveKey>(registry, "particle_curve_key")
        .field("time", &ParticleCurveKey::time).hints(ranged(0.0f, 1.0f, "of the particle's life"))
        .field("value", &ParticleCurveKey::value)
        .structure();
    TypeBuilder<ParticleCurve>(registry, "particle_curve")
        .field("keys_count", &ParticleCurve::keys_count).hints(ranged(0.0f, static_cast<float>(PARTICLE_CURVE_MAX_KEYS),
            "keys used, in time order (none: 1 all life long)"))
        .structure("keys", &ParticleCurve::keys, "particle_curve_key")
        .structure();
    TypeBuilder<ParticleColorKey>(registry, "particle_color_key")
        .field("time", &ParticleColorKey::time).hints(ranged(0.0f, 1.0f, "of the particle's life"))
        .field("color", &ParticleColorKey::color).hints(color("multiplies the start colour"))
        .structure();
    TypeBuilder<ParticleColorCurve>(registry, "particle_color_curve")
        .field("keys_count", &ParticleColorCurve::keys_count).hints(ranged(0.0f, static_cast<float>(PARTICLE_CURVE_MAX_KEYS),
            "keys used, in time order (none: white all life long)"))
        .structure("keys", &ParticleColorCurve::keys, "particle_color_key")
        .structure();

    using E = ParticleEmitterDesc;
    TypeBuilder<E>(registry, "particle_effect_emitter")
        .field("enabled", &E::enabled)
        .field("duration", &E::duration).hints(ranged(0.0f, 600.0f, "seconds of emission (a loop's length when looping)"))
        .field("looping", &E::looping).hints(tip("emits again from the start after its duration"))
        .field("rate", &E::rate).hints(ranged(0.0f, 10000.0f, "particles a second during the duration"))
        .field("bursts_count", &E::bursts_count).hints(ranged(0.0f, static_cast<float>(PARTICLE_EMITTER_MAX_BURSTS), "bursts used"))
        .structure("bursts", &E::bursts, "particle_burst")
        .field("max_particles", &E::max_particles).hints(ranged(0.0f, static_cast<float>(PARTICLE_EMITTER_CAPACITY_LIMIT),
            "live particles at most; spawns beyond it are skipped"))
        .enumeration("space", &E::space, {
            { "world", static_cast<std::int32_t>(ParticleSpace::eWorld) },
            { "local", static_cast<std::int32_t>(ParticleSpace::eLocal) } })
        .hints(tip("world: particles stay where they spawned; local: they move with the emitter"))
        .enumeration("shape", &E::shape, {
            { "point", static_cast<std::int32_t>(ParticleSpawnShape::ePoint) },
            { "sphere", static_cast<std::int32_t>(ParticleSpawnShape::eSphere) },
            { "box", static_cast<std::int32_t>(ParticleSpawnShape::eBox) } })
        .field("shape_offset", &E::shape_offset).hints(tip("the shape's centre in the effect's space"))
        .field("shape_radius", &E::shape_radius).hints(ranged(0.0f, 1000.0f, "the sphere's radius"))
        .field("shape_half_extents", &E::shape_half_extents).hints(ranged(0.0f, 1000.0f, "the box's half extents"))
        .field("shape_surface", &E::shape_surface).hints(tip("on the shape's surface instead of inside it"))
        .structure("lifetime", &E::lifetime, "particle_range")
        .structure("speed", &E::speed, "particle_range")
        .enumeration("direction_mode", &E::direction_mode, {
            { "direction", static_cast<std::int32_t>(ParticleDirectionMode::eDirection) },
            { "outward", static_cast<std::int32_t>(ParticleDirectionMode::eOutward) } })
        .hints(tip("direction: along `direction`; outward: away from the shape's centre"))
        .field("direction", &E::direction)
        .field("cone_degrees", &E::cone_degrees).hints(ranged(0.0f, 180.0f, "the spread around the direction (half angle)"))
        .structure("size", &E::size, "particle_range")
        .structure("rotation_degrees", &E::rotation_degrees, "particle_range")
        .structure("rotation_speed_degrees", &E::rotation_speed_degrees, "particle_range")
        .field("color_min", &E::color_min).hints(color("the start colour is random between the two"))
        .field("color_max", &E::color_max).hints(color("the start colour is random between the two"))
        .structure("size_over_life", &E::size_over_life, "particle_curve")
        .structure("color_over_life", &E::color_over_life, "particle_color_curve")
        .structure("alpha_over_life", &E::alpha_over_life, "particle_curve")
        .structure("erosion_over_life", &E::erosion_over_life, "particle_curve")
        .field("drag", &E::drag).hints(ranged(0.0f, 100.0f, "slows the particles down, per second"))
        .field("gravity_scale", &E::gravity_scale).hints(ranged(-10.0f, 10.0f, "of the scene's gravity"))
        .asset("texture", &E::texture, AssetKind::eTexture)
        .field("flipbook_columns", &E::flipbook_columns).hints(ranged(1.0f, 64.0f, "frames across the texture"))
        .field("flipbook_rows", &E::flipbook_rows).hints(ranged(1.0f, 64.0f, "frames down the texture"))
        .field("flipbook_fps", &E::flipbook_fps).hints(ranged(0.0f, 240.0f, "frames a second"))
        .field("flipbook_over_life", &E::flipbook_over_life).hints(tip("the frames play once over each particle's life"))
        .enumeration("blend", &E::blend, {
            { "alpha", static_cast<std::int32_t>(ParticleBlendMode::eAlpha) },
            { "premultiplied", static_cast<std::int32_t>(ParticleBlendMode::ePremultiplied) },
            { "additive", static_cast<std::int32_t>(ParticleBlendMode::eAdditive) } })
        .field("lit", &E::lit).hints(tip("lit by the scene's light"))
        .field("soft_distance", &E::soft_distance).hints(ranged(0.0f, 10.0f, "metres over which it fades into the geometry behind it"))
        .field("sort_order", &E::sort_order).hints(tip("among effects at the same distance, higher draws on top"))
        .structure();

    TypeBuilder<ParticleEffectDesc>(registry, PARTICLE_EFFECT_TYPE_NAME)
        .field("emitters_count", &ParticleEffectDesc::emitters_count).hints(ranged(0.0f, static_cast<float>(PARTICLE_EFFECT_MAX_EMITTERS), "emitters used"))
        .structure("emitters", &ParticleEffectDesc::emitters, "particle_effect_emitter")
        .data_asset(default_particle_effect());
}
}  // namespace

void engine::register_builtin_types(TypeRegistry& registry)
{
    // data asset types the engine's components reference
    register_particle_effect(registry);

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
    // the skin before what reads its pose (a controller is created when it gets its animation set on a game object
    // with a skin), and joint attachments before the colliders, whose bodies are created at the joint's world matrix
    register_skeletal_animation(registry);

    // rendering
    TypeBuilder<engine_mesh_component_t>(registry, "mesh")
        .asset("geometry", &engine_mesh_component_t::geometry, AssetKind::eGeometry)
        .field("disable", &engine_mesh_component_t::disable)
        .component();
    TypeBuilder<engine_skinned_mesh_component_t>(registry, "skinned_mesh")
        .asset("geometry", &engine_skinned_mesh_component_t::geometry, AssetKind::eGeometry)
        .game_object("skin", &engine_skinned_mesh_component_t::skin)
        .field("disable", &engine_skinned_mesh_component_t::disable)
        .component();
    TypeBuilder<engine_material_component_t>(registry, "material")
        .field("diffuse_color", &engine_material_component_t::diffuse_color)
        .asset("diffuse_texture", &engine_material_component_t::diffuse_texture, AssetKind::eTexture)
        .field("shininess", &engine_material_component_t::shininess)
        .asset("specular_texture", &engine_material_component_t::specular_texture, AssetKind::eTexture)
        .component();
    register_light(registry);
    register_camera(registry);
    TypeBuilder<engine_particle_emitter_component_t>(registry, "particle_emitter")
        .data_asset_ref("effect", &engine_particle_emitter_component_t::effect, PARTICLE_EFFECT_TYPE_NAME)
        .field("play_on_start", &engine_particle_emitter_component_t::play_on_start)
        .field("scale", &engine_particle_emitter_component_t::scale)
        .field("seed", &engine_particle_emitter_component_t::seed)
        .component();

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
        .field("layer", &engine_character_controller_component_t::layer)
        .field("collides_with", &engine_character_controller_component_t::collides_with)
        .component();
}
