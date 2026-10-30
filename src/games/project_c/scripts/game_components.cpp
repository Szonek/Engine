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

engine_field_desc_t data_asset(const char* name, std::size_t offset, const char* type_name, const char* tooltip)
{
    auto f = field(name, ENGINE_FIELD_KIND_DATA_ASSET, offset, tooltip);
    f.type_name = type_name;
    return f;
}

engine_field_desc_t structs(const char* name, std::size_t offset, const char* type_name, const char* tooltip, std::uint32_t count)
{
    auto f = field(name, ENGINE_FIELD_KIND_STRUCT, offset, tooltip, count);
    f.type_name = type_name;
    return f;
}

// a component, or a struct type data assets hold
template<typename T>
engine_component_type_t register_type(const char* name, const std::vector<engine_field_desc_t>& fields, const T& default_value, bool component,
    std::uint32_t version)
{
    engine_type_desc_t desc{};
    desc.name = name;
    desc.version = version;
    desc.size = sizeof(T);
    desc.alignment = alignof(T);
    desc.default_value = &default_value;
    desc.fields = fields.data();
    desc.fields_count = static_cast<std::uint32_t>(fields.size());
    engine_component_type_t type = ENGINE_INVALID_OBJECT_HANDLE;
    const auto result = component ? engineComponentTypeRegister(&desc, &type) : engineStructTypeRegister(&desc, &type);
    if (result != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Failed registering the game type {}.\n", name));
    }
    return type;
}

template<typename T>
engine_component_type_t register_component(const char* name, const std::vector<engine_field_desc_t>& fields, const T& default_value, std::uint32_t version = 1)
{
    return register_type(name, fields, default_value, true, version);
}

template<typename T>
engine_component_type_t register_data_asset_type(const char* name, const std::vector<engine_field_desc_t>& fields, const T& default_value, std::uint32_t version = 1)
{
    // the data asset files under assets/data record the version they were written for ("type_version")
    return register_type(name, fields, default_value, false, version);
}
}  // namespace

project_c::EnemyType project_c::default_enemy_type()
{
    // the skeleton warrior's attacks; the windows were measured by sampling the "handslot.r" joint of
    // Skeleton_Warrior.glb and checking when the blade sweeps the area in front of the skeleton:
    //  - Slice_Horizontal: wind-up to the left ends at ~18%, strike crosses the front at ~20-30%, and from 30% on the
    //    blade is in follow-through at ~80 degrees to the right (a shared 30-62% window always missed).
    //  - Slice_Diagonal: wind-up held until ~32%, strike crosses the front at ~34-46%.
    // They are slightly padded: they were measured while the blade collider still lagged one frame behind the animated
    // pose (Scene::update now computes world transforms before physics, so it no longer does).
    return EnemyType{ ENGINE_INVALID_OBJECT_HANDLE, 20, 2, 2.0f, 0.5f, 0.8f, { 1.0f, 0.32f, 0.22f }, {
        { "1H_Melee_Attack_Slice_Horizontal", 0.18f, 0.36f },
        { "1H_Melee_Attack_Slice_Diagonal", 0.32f, 0.50f },
    } };
}

project_c::PlayerTuning project_c::default_player_tuning()
{
    PlayerTuning t{};
    t.max_hp = 100;
    t.attack_damage = 10;
    t.move_speed = 2.5f;
    t.dodge_speed = 4.0f;
    t.dodge_cooldown_seconds = 0.65f;
    t.dodge_invulnerability_seconds = 0.2f;
    t.attack_playback_rate = 1.25f;
    t.attack_input_buffer_seconds = 0.25f;
    t.attack_combo_cross_fade_seconds = 0.08f;
    t.attack_exit_cross_fade_seconds = 0.15f;
    t.turn_rate_degrees_per_second = 540.0f;
    t.movement_acceleration = 24.0f;
    t.movement_braking = 30.0f;
    t.hit_stop_seconds = 0.06f;
    t.hit_camera_shake_intensity = 0.035f;
    t.hit_camera_shake_seconds = 0.08f;
    t.hit_stun_seconds = 0.0f;
    t.knockback_speed = 2.5f;
    return t;
}

project_c::EnemyConfig project_c::default_enemy_config()
{
    return EnemyConfig{ ENGINE_INVALID_OBJECT_HANDLE };
}

project_c::PlayerConfig project_c::default_player_config()
{
    return PlayerConfig{ ENGINE_INVALID_OBJECT_HANDLE };
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

project_c::LevelExitConfig project_c::default_level_exit()
{
    return LevelExitConfig{ "", "default" };
}

void project_c::register_game_components()
{
    // the data asset types first: the components reference them (and the struct types first: the data assets hold them)
    g_types.enemy_attack = register_data_asset_type("enemy_attack", {
        field("animation", ENGINE_FIELD_KIND_STRING, offsetof(EnemyAttack, animation), "the attack's animation; empty for none", ANIMATION_NAME_MAX_LENGTH),
        ranged(field("active_start", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyAttack, active_start), "normalized animation time the blade starts dealing damage"),
            0.0f, 1.0f),
        ranged(field("active_end", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyAttack, active_end), "normalized animation time the blade stops dealing damage"),
            0.0f, 1.0f),
    }, EnemyAttack{});

    g_types.enemy_type = register_data_asset_type("enemy_type", {
        field("prefab", ENGINE_FIELD_KIND_PREFAB, offsetof(EnemyType, prefab), "the model the enemies of the kind are made of"),
        ranged(field("max_hp", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyType, max_hp), "hit points of a fresh enemy"), 1.0f, 1000.0f),
        ranged(field("attack_damage", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyType, attack_damage), "damage of one hit on the player"), 0.0f, 100.0f),
        ranged(field("move_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyType, move_speed), "metres per second"), 0.0f, 10.0f),
        ranged(field("attack_range", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyType, attack_range), "metres: attacks start this close to the player"), 0.1f, 3.0f),
        ranged(field("attack_cooldown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyType, attack_cooldown_seconds), "seconds between two attacks"), 0.0f, 10.0f),
        field("hit_flash_color", ENGINE_FIELD_KIND_COLOR, offsetof(EnemyType, hit_flash_color), "the colour the enemy flashes when hit", 3),
        structs("attacks", offsetof(EnemyType, attacks), "enemy_attack", "used in turn; the ones without an animation are skipped", ENEMY_TYPE_MAX_ATTACKS),
    }, default_enemy_type());

    g_types.player_tuning = register_data_asset_type("player_tuning", {
        ranged(field("max_hp", ENGINE_FIELD_KIND_UINT32, offsetof(PlayerTuning, max_hp), "hit points at the start"), 1.0f, 1000.0f),
        ranged(field("attack_damage", ENGINE_FIELD_KIND_UINT32, offsetof(PlayerTuning, attack_damage), "damage of each dagger hit"), 0.0f, 100.0f),
        ranged(field("move_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, move_speed), "metres per second"), 0.0f, 10.0f),
        ranged(field("dodge_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, dodge_speed), "metres per second"), 0.0f, 20.0f),
        ranged(field("dodge_cooldown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, dodge_cooldown_seconds), "seconds between two dodges"), 0.0f, 10.0f),
        ranged(field("dodge_invulnerability_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, dodge_invulnerability_seconds),
            "seconds of a dodge the player can't be hit (at most the dodge)"), 0.0f, 2.0f),
        ranged(field("attack_playback_rate", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, attack_playback_rate), "speed of the attack animations"), 0.1f, 4.0f),
        ranged(field("attack_input_buffer_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, attack_input_buffer_seconds),
            "an attack pressed this long before the current one ends is the combo's next one"), 0.0f, 1.0f),
        ranged(field("attack_combo_cross_fade_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, attack_combo_cross_fade_seconds),
            "blend from one attack of the combo to the next"), 0.0f, 1.0f),
        ranged(field("attack_exit_cross_fade_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, attack_exit_cross_fade_seconds),
            "blend into the first attack and out of the last one"), 0.0f, 1.0f),
        ranged(field("turn_rate_degrees_per_second", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, turn_rate_degrees_per_second),
            "how fast the player turns toward an attack's direction"), 1.0f, 3600.0f),
        ranged(field("movement_acceleration", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, movement_acceleration),
            "metres per second per second toward the wanted speed while moving"), 0.1f, 200.0f),
        ranged(field("movement_braking", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, movement_braking),
            "metres per second per second toward stopping without input"), 0.1f, 200.0f),
        ranged(field("hit_stop_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, hit_stop_seconds), "the freeze on a hit which lands"), 0.0f, 0.5f),
        ranged(field("hit_camera_shake_intensity", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, hit_camera_shake_intensity),
            "how far the camera shakes on a hit which lands"), 0.0f, 1.0f),
        ranged(field("hit_camera_shake_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, hit_camera_shake_seconds), "how long the camera shakes"), 0.0f, 1.0f),
        ranged(field("hit_stun_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, hit_stun_seconds), "seconds without control when the player is hit"), 0.0f, 2.0f),
        ranged(field("knockback_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, knockback_speed), "metres per second the player is pushed back when hit"), 0.0f, 20.0f),
    }, default_player_tuning());

    // version 2: the tuning moved to the data assets
    g_types.enemy_config = register_component("enemy_config", {
        data_asset("type", offsetof(EnemyConfig, type), "enemy_type", "the kind of enemy: its tuning, shared by the enemies of the kind (assets/data/enemies)"),
    }, default_enemy_config(), 2);

    g_types.player_config = register_component("player_config", {
        data_asset("tuning", offsetof(PlayerConfig, tuning), "player_tuning", "the player's tuning (assets/data/player)"),
    }, default_player_config(), 2);

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

    g_types.level_exit = register_component("level_exit", {
        field("level", ENGINE_FIELD_KIND_STRING, offsetof(LevelExitConfig, level), "the level file it leads to, relative to assets/scenes", LEVEL_PATH_MAX_LENGTH),
        field("entrance", ENGINE_FIELD_KIND_STRING, offsetof(LevelExitConfig, entrance), "the id of the player start there where the player enters",
            PLAYER_START_ID_MAX_LENGTH),
    }, default_level_exit());
}

const project_c::GameComponentTypes& project_c::game_components()
{
    return g_types;
}
