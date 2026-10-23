#include "building_script.h"
#include "scripts_utils.h"
#include "player_script.h"

#include "../app.h"
#include "iscene.h"

#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <optional>

namespace
{
using project_c::utils::set_name;

// a collider box of a building part, in its mesh space (the part's local, unscaled space)
struct Box
{
    glm::vec3 center;
    glm::vec3 half_extents;
};

// Primitive_* KayKit pieces are 4x4 units in mesh space, house uses them at half size.
constexpr float PIECE_SCALE = 0.5f;
// North/South walls are slightly longer, so they cover the corners where they meet East/West walls.
constexpr float LONG_WALL_X_SCALE = PIECE_SCALE * (project_c::House::half_size + project_c::House::wall_thickness * 0.5f) / project_c::House::half_size;

// Mesh space boxes (Primitive_Wall / Primitive_Window: x [-2, 2], y [0, 4], z [-0.5, 0.5])
constexpr Box WALL_BOX = { { 0.0f, 2.0f, 0.0f }, { 2.0f, 2.0f, 0.5f } };
// Primitive_Window has an opening at x [-1, 1], y [1, 3]. Compound collider supports only single child,
// so the frame is built from 4 invisible blockers sharing the transform of the visible piece.
constexpr std::array<Box, 4> WINDOW_FRAME_BOXES = {
    Box{ { -1.5f, 2.0f, 0.0f }, { 0.5f, 2.0f, 0.5f } },  // left pillar
    Box{ {  1.5f, 2.0f, 0.0f }, { 0.5f, 2.0f, 0.5f } },  // right pillar
    Box{ {  0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.5f } },  // sill
    Box{ {  0.0f, 3.5f, 0.0f }, { 1.0f, 0.5f, 0.5f } },  // lintel
};
// Primitive_Floor: x [-2, 2], y [0, 1], z [-2, 2]
constexpr Box ROOF_BOX = { { 0.0f, 0.5f, 0.0f }, { 2.0f, 0.5f, 2.0f } };
} // namespace anonymous

project_c::BuildingPart::BuildingPart(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

project_c::BuildingPartConfig project_c::BuildingPart::config() const
{
    return get_game_component(go_, game_components().building_part_config, default_building_part_config());
}

bool project_c::BuildingPart::is_climbable(const glm::vec3& surface_normal) const
{
    const auto cfg = config();
    if (!cfg.climbable)
    {
        return false;
    }
    if (cfg.climbable_from_outside_only && engineGameObjectHasParentComponent(go_))
    {
        const glm::vec3 outward = utils::get_world_position(go_) - utils::get_world_position(engineGameObjectGetParentComponent(go_).parent);
        return surface_normal.x * outward.x + surface_normal.z * outward.z > 0.0f;
    }
    return true;
}

std::optional<glm::vec3> project_c::BuildingPart::get_alignment_point() const
{
    if (!config().align_to_position)
    {
        return std::nullopt;
    }
    return utils::get_world_position(go_);
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

    if (!affect_collider)
    {
        return;
    }
    if (hidden && !removed_collider_ && engineGameObjectHasColliderComponent(go_))
    {
        removed_collider_ = engineGameObjectGetColliderComponent(go_);
        engineGameObjectRemoveColliderComponent(go_);
    }
    else if (!hidden && removed_collider_)
    {
        engineGameObjectAddColliderComponent(go_);
        engineGameObjectUpdateColliderComponent(go_, &removed_collider_.value());
        removed_collider_.reset();
    }
}

project_c::House::House(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

glm::vec3 project_c::House::get_position() const
{
    const auto tc = engineGameObjectGetTransformComponent(go_);
    return { tc.position[0], tc.position[1], tc.position[2] };
}

glm::vec3 project_c::House::get_roof_center() const
{
    return get_position() + glm::vec3(0.0f, wall_height + roof_thickness, 0.0f);
}

bool project_c::House::add_roof_prop(engine_game_object_t go)
{
    const auto type = game_components().house_config;
    auto cfg = get_game_component(go_, type, default_house_config());
    for (auto& prop : cfg.roof_props)
    {
        if (prop == ENGINE_INVALID_GAME_OBJECT_ID)
        {
            prop = go;
            if (!engineGameObjectHasDataComponent(go_, type))
            {
                engineGameObjectAddDataComponent(go_, type, &cfg);
            }
            return engineGameObjectUpdateDataComponent(go_, type, &cfg) == ENGINE_RESULT_CODE_OK;
        }
    }
    engineLog("The house holds as many roof props as it can.\n");
    return false;
}

void project_c::House::on_start()
{
    if (const auto* player = my_scene_->find_script<Player>())
    {
        player_go_ = player->get_game_object();
    }
    for (const auto child : utils::get_children(go_))
    {
        auto* part = my_scene_->get_script<BuildingPart>(child);
        if (part && part->get_hide_when_inside() != BUILDING_PART_HIDE_NEVER)
        {
            hidden_when_inside_.push_back(part);
        }
    }
}

bool project_c::House::is_inside(const glm::vec3& p) const
{
    const auto center = get_position();
    const float interior_half_size = half_size - wall_thickness * 0.5f;
    return std::abs(p.x - center.x) < interior_half_size
        && std::abs(p.z - center.z) < interior_half_size
        && p.y < center.y + wall_height;
}

void project_c::House::update(float dt)
{
    // a scene without a player (e.g. a test scene)
    if (player_go_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }

    const auto ptc = engineGameObjectGetTransformComponent(player_go_);
    const bool inside = is_inside({ ptc.position[0], ptc.position[1], ptc.position[2] });
    if (inside == player_inside_)
    {
        return;
    }
    player_inside_ = inside;

    // classic ARPG "cut away": hide what blocks the view of the room from the top-down camera
    for (auto* part : hidden_when_inside_)
    {
        part->set_hidden(inside, part->get_hide_when_inside() == BUILDING_PART_HIDE_MESH_AND_COLLIDER);
    }
    const auto cfg = get_game_component(go_, game_components().house_config, default_house_config());
    for (const auto prop : cfg.roof_props)
    {
        if (prop != ENGINE_INVALID_GAME_OBJECT_ID && engineGameObjectIsValid(prop) && engineGameObjectHasMeshComponent(prop))
        {
            auto mc = engineGameObjectGetMeshComponent(prop);
            mc.disable = inside;
            engineGameObjectUpdateMeshComponent(prop, &mc);
        }
    }
}

project_c::House* project_c::build_house(engine::IScene& scene, AppProjectC& app, const glm::vec2& center, float ground_y)
{
    // the house stands at the center of its floor; its parts are placed relative to it
    const auto house_go = engineGameObjectCreate();
    set_name(house_go, "house");
    auto house_tc = engineGameObjectAddTransformComponent(house_go);
    house_tc.position[0] = center.x;
    house_tc.position[1] = ground_y;
    house_tc.position[2] = center.y;
    engineGameObjectUpdateTransformComponent(house_go, &house_tc);
    auto* house = scene.attach_script<House>(house_go);

    auto add_part = [&](engine_game_object_t go, const char* name, const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale,
        std::optional<Box> collider, const BuildingPartConfig& cfg)
        {
            set_name(go, name);
            auto tc = engineGameObjectHasTransformComponent(go) ? engineGameObjectGetTransformComponent(go) : engineGameObjectAddTransformComponent(go);
            set_c_array(tc.position, std::array<float, 3>{ position.x, position.y, position.z });
            const glm::quat rot = glm::angleAxis(glm::radians(y_rotation_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
            set_c_array(tc.rotation, std::array<float, 4>{ rot.x, rot.y, rot.z, rot.w });
            set_c_array(tc.scale, std::array<float, 3>{ scale.x, scale.y, scale.z });
            engineGameObjectUpdateTransformComponent(go, &tc);

            auto pc = engineGameObjectAddParentComponent(go);
            pc.parent = house_go;
            engineGameObjectUpdateParentComponent(go, &pc);

            // the collider box is in the piece's mesh space
            if (collider)
            {
                auto cc = engineGameObjectAddColliderComponent(go);
                cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
                auto& cc_child = cc.collider.compound.children[0];
                cc_child.rotation_quaternion[3] = 1.0f;
                set_c_array(cc_child.transform, std::array<float, 3>{ collider->center.x, collider->center.y, collider->center.z });
                cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
                set_c_array(cc_child.collider.box.size, std::array<float, 3>{ collider->half_extents.x, collider->half_extents.y, collider->half_extents.z });
                engineGameObjectUpdateColliderComponent(go, &cc);
            }
            engineGameObjectAddDataComponent(go, game_components().building_part_config, &cfg);
            scene.attach_script<BuildingPart>(go);
        };
    const auto part_config = [](bool climbable, bool camera_facing, bool align_to_position)
        {
            auto cfg = default_building_part_config();
            cfg.climbable = climbable;
            // walls are climbed from the outside only
            cfg.climbable_from_outside_only = climbable;
            cfg.align_to_position = align_to_position;
            cfg.hide_when_inside = camera_facing ? BUILDING_PART_HIDE_MESH : BUILDING_PART_HIDE_NEVER;
            return cfg;
        };
    auto add_wall = [&](const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale, bool camera_facing)
        {
            if (app.is_prefab_available(PREFAB_TYPE_BUILDING_WALL))
            {
                add_part(app.instantiate_prefab(PREFAB_TYPE_BUILDING_WALL).go, "house-wall", position, y_rotation_degrees, scale, WALL_BOX,
                    part_config(true, camera_facing, false));
            }
        };
    auto add_window_wall = [&](const glm::vec3& position, float y_rotation_degrees, const glm::vec3& scale, bool camera_facing)
        {
            if (!app.is_prefab_available(PREFAB_TYPE_BUILDING_WINDOW))
            {
                return;
            }
            // visible piece, without collider
            add_part(app.instantiate_prefab(PREFAB_TYPE_BUILDING_WINDOW).go, "house-window", position, y_rotation_degrees, scale, std::nullopt,
                part_config(false, camera_facing, false));

            // invisible collision frame around the opening, aligning traversal to the opening
            for (const auto& box : WINDOW_FRAME_BOXES)
            {
                add_part(engineGameObjectCreate(), "house-window-frame", position, y_rotation_degrees, scale, box, part_config(true, false, true));
            }
        };

    const glm::vec3 long_wall_scale(LONG_WALL_X_SCALE, PIECE_SCALE, PIECE_SCALE);
    const glm::vec3 short_wall_scale(PIECE_SCALE);
    const float long_wall_offset = House::half_size * LONG_WALL_X_SCALE / PIECE_SCALE * 0.5f;  // center of each half of the long wall
    const float short_wall_offset = House::half_size * 0.5f;

    // north (back) wall
    add_wall({ -long_wall_offset, 0.0f, -House::half_size }, 0.0f, long_wall_scale, false);
    add_wall({ long_wall_offset, 0.0f, -House::half_size }, 0.0f, long_wall_scale, false);
    // south (front, camera facing) wall with the window
    add_window_wall({ -long_wall_offset, 0.0f, House::half_size }, 0.0f, long_wall_scale, true);
    add_wall({ long_wall_offset, 0.0f, House::half_size }, 0.0f, long_wall_scale, true);
    // west and east walls
    add_wall({ -House::half_size, 0.0f, -short_wall_offset }, 90.0f, short_wall_scale, false);
    add_wall({ -House::half_size, 0.0f, short_wall_offset }, 90.0f, short_wall_scale, false);
    add_wall({ House::half_size, 0.0f, -short_wall_offset }, 90.0f, short_wall_scale, false);
    add_wall({ House::half_size, 0.0f, short_wall_offset }, 90.0f, short_wall_scale, false);

    if (app.is_prefab_available(PREFAB_TYPE_BUILDING_ROOF))
    {
        // roof covers walls fully, so its edge is flush with the outer face of the walls
        const float roof_xz_scale = (House::half_size + House::wall_thickness * 0.5f) / 2.0f;
        const float roof_y_scale = House::roof_thickness / (ROOF_BOX.half_extents.y * 2.0f);
        // its collider goes too, so mouse aiming/picking hits the room floor instead of the roof
        auto roof = part_config(false, false, false);
        roof.hide_when_inside = BUILDING_PART_HIDE_MESH_AND_COLLIDER;
        add_part(app.instantiate_prefab(PREFAB_TYPE_BUILDING_ROOF).go, "house-roof", glm::vec3(0.0f, House::wall_height, 0.0f), 0.0f,
            glm::vec3(roof_xz_scale, roof_y_scale, roof_xz_scale), ROOF_BOX, roof);
    }
    return house;
}
