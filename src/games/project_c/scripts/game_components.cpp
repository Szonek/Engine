#include "game_components.h"

#include <utils.h>

#include <fmt/format.h>

#include <cstddef>
#include <vector>

namespace
{
project_c::GameComponentTypes g_types;

engine_field_desc_t field(const char* name, engine_field_kind_t kind, std::size_t offset, const char* tooltip, std::uint32_t count = 0)
{
    engine_field_desc_t f{};
    f.name = name;
    f.kind = kind;
    f.offset = static_cast<std::uint32_t>(offset);
    f.count = count;
    f.tooltip = tooltip;
    return f;
}

engine_field_desc_t ranged(engine_field_desc_t f, float min_value, float max_value)
{
    f.flags |= ENGINE_FIELD_FLAG_RANGE;
    f.range_min = min_value;
    f.range_max = max_value;
    return f;
}

template<typename T>
engine_component_type_t register_component(const char* name, const std::vector<engine_field_desc_t>& fields, const T& default_value)
{
    engine_type_desc_t desc{};
    desc.name = name;
    desc.size = sizeof(T);
    desc.alignment = alignof(T);
    desc.default_value = &default_value;
    desc.fields = fields.data();
    desc.fields_count = static_cast<std::uint32_t>(fields.size());
    engine_component_type_t type = ENGINE_INVALID_OBJECT_HANDLE;
    if (engineComponentTypeRegister(&desc, &type) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Failed registering the game component {}.\n", name));
    }
    return type;
}
}  // namespace

project_c::EnemyConfig project_c::default_enemy_config()
{
    return EnemyConfig{ 20, 2, 2.0f, 0.5f, 0.8f, { 1.0f, 0.32f, 0.22f } };
}

project_c::PlayerConfig project_c::default_player_config()
{
    return PlayerConfig{ 100, 10, 2.5f, 4.0f, 0.65f };
}

project_c::BuildingPartConfig project_c::default_building_part_config()
{
    return BuildingPartConfig{ false, false, false, BUILDING_PART_HIDE_NEVER };
}

project_c::HouseConfig project_c::default_house_config()
{
    return HouseConfig{};
}

project_c::PlayerStart project_c::default_player_start()
{
    return PlayerStart{ "default" };
}

void project_c::register_game_components()
{
    g_types.enemy_config = register_component("enemy_config", {
        ranged(field("max_hp", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyConfig, max_hp), "hit points of a fresh enemy"), 1.0f, 1000.0f),
        ranged(field("attack_damage", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyConfig, attack_damage), "damage of one hit on the player"), 0.0f, 100.0f),
        ranged(field("move_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyConfig, move_speed), "metres per second"), 0.0f, 10.0f),
        ranged(field("attack_range", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyConfig, attack_range), "metres: attacks start this close to the player"), 0.1f, 3.0f),
        ranged(field("attack_cooldown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyConfig, attack_cooldown_seconds), "seconds between two attacks"), 0.0f, 10.0f),
        field("hit_flash_color", ENGINE_FIELD_KIND_COLOR, offsetof(EnemyConfig, hit_flash_color), "the colour the enemy flashes when hit", 3),
    }, default_enemy_config());

    g_types.player_config = register_component("player_config", {
        ranged(field("max_hp", ENGINE_FIELD_KIND_UINT32, offsetof(PlayerConfig, max_hp), "hit points at the start"), 1.0f, 1000.0f),
        ranged(field("attack_damage", ENGINE_FIELD_KIND_UINT32, offsetof(PlayerConfig, attack_damage), "damage of each dagger hit"), 0.0f, 100.0f),
        ranged(field("move_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerConfig, move_speed), "metres per second"), 0.0f, 10.0f),
        ranged(field("dodge_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerConfig, dodge_speed), "metres per second"), 0.0f, 20.0f),
        ranged(field("dodge_cooldown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerConfig, dodge_cooldown_seconds), "seconds between two dodges"), 0.0f, 10.0f),
    }, default_player_config());

    static const engine_enum_value_desc_t hide_values[] = {
        { "never", BUILDING_PART_HIDE_NEVER },
        { "mesh", BUILDING_PART_HIDE_MESH },
        { "mesh_and_collider", BUILDING_PART_HIDE_MESH_AND_COLLIDER },
    };
    auto hide = field("hide_when_inside", ENGINE_FIELD_KIND_ENUM, offsetof(BuildingPartConfig, hide_when_inside), "what the house hides while the player is inside");
    hide.enum_values = hide_values;
    hide.enum_values_count = static_cast<std::uint32_t>(std::size(hide_values));
    g_types.building_part_config = register_component("building_part_config", {
        field("climbable", ENGINE_FIELD_KIND_BOOL, offsetof(BuildingPartConfig, climbable), "the player can climb its colliders"),
        field("climbable_from_outside_only", ENGINE_FIELD_KIND_BOOL, offsetof(BuildingPartConfig, climbable_from_outside_only),
            "only the faces pointing away from its parent (the house) are climbable"),
        field("align_to_position", ENGINE_FIELD_KIND_BOOL, offsetof(BuildingPartConfig, align_to_position), "traversal aligns to its position (a window opening)"),
        hide,
    }, default_building_part_config());

    g_types.house_config = register_component("house_config", {
        field("roof_props", ENGINE_FIELD_KIND_GAME_OBJECT, offsetof(HouseConfig, roof_props), "objects on the roof, hidden with it", HOUSE_MAX_ROOF_PROPS),
    }, default_house_config());

    g_types.player_start = register_component("player_start", {
        field("id", ENGINE_FIELD_KIND_STRING, offsetof(PlayerStart, id), "the entrance's name; \"default\" is where the level starts", PLAYER_START_ID_MAX_LENGTH),
    }, default_player_start());
}

const project_c::GameComponentTypes& project_c::game_components()
{
    return g_types;
}
