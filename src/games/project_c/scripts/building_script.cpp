#include "building_script.h"
#include "scripts_utils.h"

#include "../app.h"
#include "iscene.h"

#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>

namespace
{
// Primitive_* KayKit pieces are 4x4 units in mesh space, house uses them at half size.
constexpr float PIECE_SCALE = 0.5f;
// North/South walls are slightly longer, so they cover the corners where they meet East/West walls.
constexpr float LONG_WALL_X_SCALE = PIECE_SCALE * (project_c::House::half_size + project_c::House::wall_thickness * 0.5f) / project_c::House::half_size;

// Mesh space boxes (Primitive_Wall / Primitive_Window: x [-2, 2], y [0, 4], z [-0.5, 0.5])
constexpr project_c::BuildingPart::Box WALL_BOX = { { 0.0f, 2.0f, 0.0f }, { 2.0f, 2.0f, 0.5f } };
// Primitive_Window has an opening at x [-1, 1], y [1, 3]. Compound collider supports only single child,
// so the frame is built from 4 invisible blockers sharing the transform of the visible piece.
constexpr std::array<project_c::BuildingPart::Box, 4> WINDOW_FRAME_BOXES = {
    project_c::BuildingPart::Box{ { -1.5f, 2.0f, 0.0f }, { 0.5f, 2.0f, 0.5f } },  // left pillar
    project_c::BuildingPart::Box{ {  1.5f, 2.0f, 0.0f }, { 0.5f, 2.0f, 0.5f } },  // right pillar
    project_c::BuildingPart::Box{ {  0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.5f } },  // sill
    project_c::BuildingPart::Box{ {  0.0f, 3.5f, 0.0f }, { 1.0f, 0.5f, 0.5f } },  // lintel
};
// Primitive_Floor: x [-2, 2], y [0, 1], z [-2, 2]
constexpr project_c::BuildingPart::Box ROOF_BOX = { { 0.0f, 0.5f, 0.0f }, { 2.0f, 0.5f, 2.0f } };
} // namespace anonymous

project_c::BuildingPart::BuildingPart(engine::IScene* my_scene, engine_game_object_t go, std::string_view name,
    const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale,
    std::optional<Box> collider, bool climbable)
    : BaseNode(my_scene, go, name)
    , collider_(collider)
    , position_(position)
    , climbable_(climbable)
{
    auto tc = engineGameObjectHasTransformComponent(go_) ? engineGameObjectGetTransformComponent(go_) : engineGameObjectAddTransformComponent(go_);
    tc.position[0] = position.x;
    tc.position[1] = position.y;
    tc.position[2] = position.z;
    const glm::quat rot = glm::angleAxis(glm::radians(y_rotation_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
    tc.rotation[0] = rot.x;
    tc.rotation[1] = rot.y;
    tc.rotation[2] = rot.z;
    tc.rotation[3] = rot.w;
    tc.scale[0] = scale.x;
    tc.scale[1] = scale.y;
    tc.scale[2] = scale.z;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    if (collider_)
    {
        add_collider();
    }
}

void project_c::BuildingPart::add_collider()
{
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = false;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[0] = collider_->center.x;
    cc_child.transform[1] = collider_->center.y;
    cc_child.transform[2] = collider_->center.z;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ collider_->half_extents.x, collider_->half_extents.y, collider_->half_extents.z });
    engineGameObjectUpdateColliderComponent(go_, &cc);
}

bool project_c::BuildingPart::is_climbable(const glm::vec3& surface_normal) const
{
    if (!climbable_)
    {
        return false;
    }
    if (climbable_away_from_)
    {
        const glm::vec3 outward = position_ - climbable_away_from_.value();
        return surface_normal.x * outward.x + surface_normal.z * outward.z > 0.0f;
    }
    return true;
}

void project_c::BuildingPart::get_fade_targets(std::vector<engine_game_object_t>& out) const
{
    out.push_back(fade_target_ != ENGINE_INVALID_GAME_OBJECT_ID ? fade_target_ : go_);
    out.insert(out.end(), extra_fade_targets_.begin(), extra_fade_targets_.end());
}

void project_c::BuildingPart::set_hidden(bool hidden, bool affect_collider)
{
    if (hidden_ != hidden && engineGameObjectHasMeshComponent(go_))
    {
        auto mc = engineGameObjectGetMeshComponent(go_);
        mc.disable = hidden;
        engineGameObjectUpdateMeshComponent(go_, &mc);
    }
    hidden_ = hidden;

    if (!affect_collider || !collider_)
    {
        return;
    }
    if (hidden && !collider_removed_)
    {
        engineGameObjectRemoveColliderComponent(go_);
        collider_removed_ = true;
    }
    else if (!hidden && collider_removed_)
    {
        add_collider();
        collider_removed_ = false;
    }
}

project_c::House::House(engine::IScene* my_scene, AppProjectC& app, const glm::vec2& center, float ground_y)
    : BaseNode(my_scene, "house")
    , center_(center)
    , ground_y_(ground_y)
{
    const glm::vec3 long_wall_scale(LONG_WALL_X_SCALE, PIECE_SCALE, PIECE_SCALE);
    const glm::vec3 short_wall_scale(PIECE_SCALE);
    const float long_wall_offset = half_size * LONG_WALL_X_SCALE / PIECE_SCALE * 0.5f;  // center of each half of the long wall
    const float short_wall_offset = half_size * 0.5f;

    // north (back) wall
    add_wall(app, { center_.x - long_wall_offset, ground_y_, center_.y - half_size }, 0.0f, long_wall_scale, false);
    add_wall(app, { center_.x + long_wall_offset, ground_y_, center_.y - half_size }, 0.0f, long_wall_scale, false);
    // south (front, camera facing) wall with the window
    add_window_wall(app, { center_.x - long_wall_offset, ground_y_, center_.y + half_size }, 0.0f, long_wall_scale, true);
    add_wall(app, { center_.x + long_wall_offset, ground_y_, center_.y + half_size }, 0.0f, long_wall_scale, true);
    // west and east walls
    add_wall(app, { center_.x - half_size, ground_y_, center_.y - short_wall_offset }, 90.0f, short_wall_scale, false);
    add_wall(app, { center_.x - half_size, ground_y_, center_.y + short_wall_offset }, 90.0f, short_wall_scale, false);
    add_wall(app, { center_.x + half_size, ground_y_, center_.y - short_wall_offset }, 90.0f, short_wall_scale, false);
    add_wall(app, { center_.x + half_size, ground_y_, center_.y + short_wall_offset }, 90.0f, short_wall_scale, false);

    add_roof(app);
}

void project_c::House::add_wall(AppProjectC& app, const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale, bool camera_facing)
{
    if (!app.is_prefab_available(PREFAB_TYPE_BUILDING_WALL))
    {
        return;
    }
    auto part = my_scene_->register_script<BuildingPart>(app.instantiate_prefab(PREFAB_TYPE_BUILDING_WALL, my_scene_).go,
        "house-wall", position, y_rotation_degrees, scale, WALL_BOX, true);
    part->set_climbable_away_from({ center_.x, ground_y_, center_.y });
    if (camera_facing)
    {
        camera_occluders_.push_back(part);
    }
}

void project_c::House::add_window_wall(AppProjectC& app, const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale, bool camera_facing)
{
    if (!app.is_prefab_available(PREFAB_TYPE_BUILDING_WINDOW))
    {
        return;
    }
    // visible piece, without collider
    auto part = my_scene_->register_script<BuildingPart>(app.instantiate_prefab(PREFAB_TYPE_BUILDING_WINDOW, my_scene_).go,
        "house-window", position, y_rotation_degrees, scale, std::nullopt, false);
    if (camera_facing)
    {
        camera_occluders_.push_back(part);
    }

    // invisible collision frame around the opening
    for (const auto& box : WINDOW_FRAME_BOXES)
    {
        auto frame = my_scene_->register_script<BuildingPart>(engineGameObjectCreate(), "house-window-frame", position, y_rotation_degrees, scale, box, true);
        frame->set_alignment_point(position);
        frame->set_climbable_away_from({ center_.x, ground_y_, center_.y });
        frame->set_fade_target(part->get_game_object());
    }
}

void project_c::House::add_roof(AppProjectC& app)
{
    if (!app.is_prefab_available(PREFAB_TYPE_BUILDING_ROOF))
    {
        return;
    }
    // roof covers walls fully, so its edge is flush with the outer face of the walls
    const float roof_xz_scale = (half_size + wall_thickness * 0.5f) / 2.0f;
    const float roof_y_scale = roof_thickness / (ROOF_BOX.half_extents.y * 2.0f);
    roof_ = my_scene_->register_script<BuildingPart>(app.instantiate_prefab(PREFAB_TYPE_BUILDING_ROOF, my_scene_).go,
        "house-roof", glm::vec3(center_.x, ground_y_ + wall_height, center_.y), 0.0f, glm::vec3(roof_xz_scale, roof_y_scale, roof_xz_scale),
        ROOF_BOX, false);
}

void project_c::House::add_roof_prop(engine_game_object_t go)
{
    roof_props_.push_back(go);
    if (roof_)
    {
        roof_->add_extra_fade_target(go);
    }
}

bool project_c::House::is_inside(const glm::vec3& p) const
{
    const float interior_half_size = half_size - wall_thickness * 0.5f;
    return std::abs(p.x - center_.x) < interior_half_size
        && std::abs(p.z - center_.y) < interior_half_size
        && p.y < ground_y_ + wall_height;
}

void project_c::House::update(float dt)
{
    if (player_go_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        const auto players = utils::get_game_objects_with_name("player");
        if (players.empty())
        {
            return;
        }
        player_go_ = players.front();
    }
    if (!camera_)
    {
        const auto cameras = utils::get_game_objects_with_name("camera");
        camera_ = cameras.empty() ? nullptr : my_scene_->get_script<CameraScript>(cameras.front());
    }

    const auto ptc = engineGameObjectGetTransformComponent(player_go_);
    const bool inside = is_inside({ ptc.position[0], ptc.position[1], ptc.position[2] });
    if (inside == player_inside_)
    {
        return;
    }
    player_inside_ = inside;

    // classic ARPG "cut away": roof is hidden while the player is inside,
    // its collider is removed as well, so mouse aiming/picking hits the room floor instead of the roof
    if (roof_)
    {
        roof_->set_hidden(inside, true);
    }
    // wall facing the camera stays, but faded (camera owns fading, so it is asked to keep it faded)
    if (camera_)
    {
        for (auto* occluder : camera_occluders_)
        {
            camera_->set_forced_fade(occluder->get_game_object(), inside);
        }
    }
    for (const auto prop : roof_props_)
    {
        if (engineGameObjectHasMeshComponent(prop))
        {
            auto mc = engineGameObjectGetMeshComponent(prop);
            mc.disable = inside;
            engineGameObjectUpdateMeshComponent(prop, &mc);
        }
    }
}
