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

project_c::EnemyPerception project_c::default_enemy_perception()
{
    EnemyPerception p{};
    p.sight_range = 7.0f;
    p.sight_half_angle_degrees = 55.0f;
    p.close_range = 0.8f;
    p.eye_height = 0.6f;
    p.detect_seconds_near = 0.25f;
    p.detect_seconds_far = 1.5f;
    p.above_sight_multiplier = 0.35f;
    p.hearing_fill_seconds = 1.0f;
    p.loud_noise_fill = 0.6f;
    p.meter_decay_seconds = 3.0f;
    p.lose_seconds = 4.0f;
    p.suspicious_move_speed = 0.8f;
    p.lose_sight_grace_seconds = 0.5f;
    return p;
}

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
    }, default_enemy_perception(), true, FallDamage{ 1.0f, 2.0f } };
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
    t.sneak_speed = 1.2f;
    t.footstep_noise_radius = 2.5f;
    t.loud_noise_radius = 5.0f;
    t.show_footstep_ring = true;
    t.footstep_ring_interval_seconds = 0.4f;
    t.shove_cooldown_seconds = 0.8f;
    t.shove_range = 1.2f;
    t.shove_half_angle_degrees = 50.0f;
    t.shove_speed = 7.0f;
    t.shove_stagger_seconds = 0.6f;
    t.shove_damage = 0;
    return t;
}

project_c::TraversalTuning project_c::default_traversal_tuning()
{
    TraversalTuning t{};
    t.climb_speed_vertical = 3.0f;
    t.climb_speed_lateral = 1.6f;
    t.mantle_duration_seconds = 0.3f;
    t.vault_duration_seconds = 0.45f;
    t.climb_down_duration_seconds = 0.35f;
    t.run_min_speed = 2.0f;
    t.run_up_max_height = 2.4f;
    t.run_up_duration_seconds = 0.5f;
    t.climb_intent_seconds = 0.1f;
    t.max_safe_drop = 3.0f;
    t.leap_distance = 3.0f;
    t.leap_height = 0.6f;
    t.air_control = 0.3f;
    t.landing_recovery_seconds = 0.15f;
    t.fall = FallDamage{ 3.0f, 8.0f };
    t.roll_window_seconds = 0.25f;
    return t;
}

project_c::ExecutionTuning project_c::default_execution_tuning()
{
    ExecutionTuning t{};
    t.execution_range = 1.4f;
    t.execution_behind_half_angle_degrees = 70.0f;
    t.execution_lunge_seconds = 0.15f;
    t.execution_seconds = 0.55f;
    t.execution_hit_stop_seconds = 0.1f;
    t.execution_damage = 60.0f;
    t.execution_noise_radius = 2.5f;
    t.aerial_min_drop = 1.0f;
    t.aerial_aim_radius = 2.5f;
    t.plunge_speed = 14.0f;
    t.knockdown_radius_per_metre = 0.6f;
    t.knockdown_max_radius = 3.0f;
    t.knockdown_seconds = 1.5f;
    t.chain_window_seconds = 1.0f;
    t.chain_range = 6.0f;
    t.chain_dash_speed = 12.0f;
    return t;
}

project_c::MomentumTuning project_c::default_momentum_tuning()
{
    return MomentumTuning{ 0.25f, 0.35f, 0.15f, 0.25f, 0.25f, 0.5f, 0.15f, 0.2f, 8.0f };
}

project_c::SmokeTuning project_c::default_smoke_tuning()
{
    return SmokeTuning{ 12.0f, 8.0f, 3.0f, 1.0f, 0.25f, 6.0f, 0.5f, 3.0f, 5.0f, 0.35f, 0.25f };
}

project_c::EnemyConfig project_c::default_enemy_config()
{
    return EnemyConfig{ ENGINE_INVALID_OBJECT_HANDLE };
}

project_c::PlayerConfig project_c::default_player_config()
{
    return PlayerConfig{ ENGINE_INVALID_OBJECT_HANDLE, ENGINE_INVALID_OBJECT_HANDLE, ENGINE_INVALID_OBJECT_HANDLE, ENGINE_INVALID_OBJECT_HANDLE,
        ENGINE_INVALID_OBJECT_HANDLE };
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

project_c::EnemySpawnerConfig project_c::default_enemy_spawner()
{
    return EnemySpawnerConfig{ ENGINE_INVALID_OBJECT_HANDLE, 3, { 2.0f, 2.0f }, 1.0f, 1 };
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

    const auto perception_field = [](const char* name, std::size_t offset, const char* tooltip, float max_value)
        {
            return ranged(field(name, ENGINE_FIELD_KIND_FLOAT, offset, tooltip), 0.0f, max_value);
        };
    g_types.enemy_perception = register_data_asset_type("enemy_perception", {
        perception_field("sight_range", offsetof(EnemyPerception, sight_range), "metres: how far it sees in its sight cone", 50.0f),
        perception_field("sight_half_angle_degrees", offsetof(EnemyPerception, sight_half_angle_degrees), "half the sight cone's angle, from its facing", 180.0f),
        perception_field("close_range", offsetof(EnemyPerception, close_range), "metres: it sees the player this close in any direction", 10.0f),
        perception_field("eye_height", offsetof(EnemyPerception, eye_height), "metres above its feet it sees from", 5.0f),
        perception_field("detect_seconds_near", offsetof(EnemyPerception, detect_seconds_near),
            "seconds the meter takes to fill while it sees the player right next to it", 10.0f),
        perception_field("detect_seconds_far", offsetof(EnemyPerception, detect_seconds_far),
            "seconds the meter takes to fill while it sees the player at sight_range (linear in between)", 10.0f),
        perception_field("above_sight_multiplier", offsetof(EnemyPerception, above_sight_multiplier),
            "a player above its reach (on a roof) fills the meter this many times as fast; below 1 is slower", 5.0f),
        perception_field("hearing_fill_seconds", offsetof(EnemyPerception, hearing_fill_seconds),
            "seconds heard footsteps take to fill the meter", 10.0f),
        perception_field("loud_noise_fill", offsetof(EnemyPerception, loud_noise_fill),
            "what a heard loud noise (attack, dodge, landing) adds to the meter at once", 1.0f),
        perception_field("meter_decay_seconds", offsetof(EnemyPerception, meter_decay_seconds),
            "a full meter empties in this long when it perceives nothing", 60.0f),
        perception_field("lose_seconds", offsetof(EnemyPerception, lose_seconds),
            "seconds it searches for a lost player; its pack calms down when no member perceived the player this long", 60.0f),
        perception_field("lose_sight_grace_seconds", offsetof(EnemyPerception, lose_sight_grace_seconds),
            "seconds an alert enemy keeps hunting a player it stopped perceiving before it searches", 10.0f),
        perception_field("suspicious_move_speed", offsetof(EnemyPerception, suspicious_move_speed),
            "metres per second while it walks to what it noticed, or searches", 10.0f),
    }, default_enemy_perception());

    g_types.fall_damage = register_data_asset_type("fall_damage", {
        ranged(field("min_height", ENGINE_FIELD_KIND_FLOAT, offsetof(FallDamage, min_height), "metres: a drop up to this height does no damage"),
            0.0f, 100.0f),
        ranged(field("lethal_height", ENGINE_FIELD_KIND_FLOAT, offsetof(FallDamage, lethal_height),
            "metres: a drop from this height or higher kills (linear in between)"), 0.0f, 100.0f),
    }, FallDamage{ 1.0f, 2.0f });

    g_types.enemy_type = register_data_asset_type("enemy_type", {
        field("prefab", ENGINE_FIELD_KIND_PREFAB, offsetof(EnemyType, prefab), "the model the enemies of the kind are made of"),
        ranged(field("max_hp", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyType, max_hp), "hit points of a fresh enemy"), 1.0f, 1000.0f),
        ranged(field("attack_damage", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyType, attack_damage), "damage of one hit on the player"), 0.0f, 100.0f),
        ranged(field("move_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyType, move_speed), "metres per second"), 0.0f, 10.0f),
        ranged(field("attack_range", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyType, attack_range), "metres: attacks start this close to the player"), 0.1f, 3.0f),
        ranged(field("attack_cooldown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyType, attack_cooldown_seconds), "seconds between two attacks"), 0.0f, 10.0f),
        field("hit_flash_color", ENGINE_FIELD_KIND_COLOR, offsetof(EnemyType, hit_flash_color), "the colour the enemy flashes when hit", 3),
        structs("attacks", offsetof(EnemyType, attacks), "enemy_attack", "used in turn; the ones without an animation are skipped", ENEMY_TYPE_MAX_ATTACKS),
        structs("perception", offsetof(EnemyType, perception), "enemy_perception", "how it notices the player: sight, hearing, how fast it forgets", 0),
        field("execution_kills", ENGINE_FIELD_KIND_BOOL, offsetof(EnemyType, execution_kills),
            "an execution kills it; otherwise it takes the execution's damage and is alerted (heavies, the target)"),
        structs("fall", offsetof(EnemyType, fall), "fall_damage", "the drops which hurt and kill it (a kick off a roof)", 0),
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
        ranged(field("sneak_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, sneak_speed),
            "metres per second while sneaking (Shift); keep it below the traversal's run_min_speed"), 0.0f, 10.0f),
        ranged(field("footstep_noise_radius", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, footstep_noise_radius),
            "metres: enemies this close hear the player walk (not sneaking)"), 0.0f, 30.0f),
        ranged(field("loud_noise_radius", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, loud_noise_radius),
            "metres: enemies this close hear an attack, a dodge or a hard landing"), 0.0f, 30.0f),
        field("show_footstep_ring", ENGINE_FIELD_KIND_BOOL, offsetof(PlayerTuning, show_footstep_ring),
            "a faint ring at the footstep radius pulses with the steps while the player walks (none while sneaking)"),
        ranged(field("footstep_ring_interval_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, footstep_ring_interval_seconds),
            "seconds between two pulses of the footstep ring"), 0.05f, 5.0f),
        ranged(field("shove_cooldown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, shove_cooldown_seconds),
            "seconds between two kicks (Q)"), 0.0f, 10.0f),
        ranged(field("shove_range", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, shove_range),
            "metres: the enemy kicked is the nearest this close"), 0.0f, 5.0f),
        ranged(field("shove_half_angle_degrees", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, shove_half_angle_degrees),
            "degrees off the player's facing an enemy can be to be kicked"), 0.0f, 180.0f),
        ranged(field("shove_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, shove_speed),
            "metres per second the kicked enemy is pushed (the push decays)"), 0.0f, 30.0f),
        ranged(field("shove_stagger_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(PlayerTuning, shove_stagger_seconds),
            "seconds the kicked enemy can neither move nor attack"), 0.0f, 5.0f),
        ranged(field("shove_damage", ENGINE_FIELD_KIND_UINT32, offsetof(PlayerTuning, shove_damage),
            "damage of the kick itself; the fall is the weapon"), 0.0f, 100.0f),
    }, default_player_tuning());

    g_types.traversal_tuning = register_data_asset_type("traversal_tuning", {
        ranged(field("climb_speed_vertical", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, climb_speed_vertical),
            "metres per second up and down a wall"), 0.1f, 10.0f),
        ranged(field("climb_speed_lateral", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, climb_speed_lateral),
            "metres per second sideways along a wall"), 0.1f, 10.0f),
        ranged(field("mantle_duration_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, mantle_duration_seconds),
            "pulling up from the top of a wall onto the ledge"), 0.05f, 2.0f),
        ranged(field("vault_duration_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, vault_duration_seconds),
            "vaulting through a window or over a low wall"), 0.05f, 2.0f),
        ranged(field("climb_down_duration_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, climb_down_duration_seconds),
            "stepping over a roof edge onto the wall below"), 0.05f, 2.0f),
        ranged(field("run_min_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, run_min_speed),
            "metres per second: approaching faster, the player runs up walls and off edges instead of climbing"), 0.0f, 10.0f),
        ranged(field("run_up_max_height", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, run_up_max_height),
            "metres: the highest ledge a run-up reaches; a higher wall is run up this far, then climbed"), 0.0f, 10.0f),
        ranged(field("run_up_duration_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, run_up_duration_seconds),
            "running up a wall and over its edge"), 0.05f, 2.0f),
        ranged(field("climb_intent_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, climb_intent_seconds),
            "walking into a wall climbs it once the input has pushed into it this long"), 0.0f, 1.0f),
        ranged(field("max_safe_drop", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, max_safe_drop),
            "metres: a higher edge is always climbed down, also when running"), 0.0f, 20.0f),
        ranged(field("leap_distance", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, leap_distance),
            "metres from the takeoff to the landing of a leap (a dodge at an edge)"), 0.0f, 20.0f),
        ranged(field("leap_height", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, leap_height),
            "metres above the takeoff at the top of a leap"), 0.0f, 5.0f),
        ranged(field("air_control", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, air_control),
            "the fraction of the ground acceleration the input has in the air"), 0.0f, 1.0f),
        ranged(field("landing_recovery_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, landing_recovery_seconds),
            "seconds without control after landing"), 0.0f, 2.0f),
        structs("fall", offsetof(TraversalTuning, fall), "fall_damage", "the drops which hurt and kill the player (keep min_height at max_safe_drop)", 0),
        ranged(field("roll_window_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(TraversalTuning, roll_window_seconds),
            "a dodge pressed this long before landing is a roll: half the fall damage"), 0.0f, 2.0f),
    }, default_traversal_tuning());

    const auto momentum_field = [](const char* name, std::size_t offset, const char* tooltip, float max_value)
        {
            return ranged(field(name, ENGINE_FIELD_KIND_FLOAT, offset, tooltip), 0.0f, max_value);
        };
    g_types.momentum_tuning = register_data_asset_type("momentum_tuning", {
        momentum_field("ground_execution_fill", offsetof(MomentumTuning, ground_execution_fill), "what an execution from behind fills the meter by", 1.0f),
        momentum_field("plunge_fill", offsetof(MomentumTuning, plunge_fill), "what a plunge fills the meter by", 1.0f),
        momentum_field("chain_link_fill", offsetof(MomentumTuning, chain_link_fill), "what each execution after the first of a chain fills it by", 1.0f),
        momentum_field("fall_kill_fill", offsetof(MomentumTuning, fall_kill_fill), "what an enemy the player kicked dying from its fall fills it by", 1.0f),
        momentum_field("speed_bonus", offsetof(MomentumTuning, speed_bonus), "move speed x (1 + this) at a full meter", 2.0f),
        momentum_field("chain_range_bonus", offsetof(MomentumTuning, chain_range_bonus), "chain range x (1 + this) at a full meter", 2.0f),
        momentum_field("combat_drain_per_second", offsetof(MomentumTuning, combat_drain_per_second),
            "drained per second on the ground in combat (an alert enemy within combat_radius)", 2.0f),
        momentum_field("hit_drain", offsetof(MomentumTuning, hit_drain), "drained at once by a hit taken", 1.0f),
        momentum_field("combat_radius", offsetof(MomentumTuning, combat_radius), "metres: an alert enemy this close means combat", 50.0f),
    }, default_momentum_tuning());

    const auto smoke_field = [](const char* name, std::size_t offset, const char* tooltip, float max_value)
        {
            return ranged(field(name, ENGINE_FIELD_KIND_FLOAT, offset, tooltip), 0.0f, max_value);
        };
    g_types.smoke_tuning = register_data_asset_type("smoke_tuning", {
        smoke_field("cooldown_seconds", offsetof(SmokeTuning, cooldown_seconds), "seconds from a throw until the next one", 120.0f),
        smoke_field("throw_range", offsetof(SmokeTuning, throw_range), "metres: the farthest the bomb lands from the player", 30.0f),
        smoke_field("radius", offsetof(SmokeTuning, radius), "metres: the cloud's radius", 20.0f),
        smoke_field("centre_height", offsetof(SmokeTuning, centre_height), "metres: the cloud's centre above where the bomb landed", 10.0f),
        smoke_field("grow_seconds", offsetof(SmokeTuning, grow_seconds), "seconds the cloud takes to grow to its radius", 5.0f),
        smoke_field("duration_seconds", offsetof(SmokeTuning, duration_seconds), "seconds the cloud lasts in all", 60.0f),
        smoke_field("fade_seconds", offsetof(SmokeTuning, fade_seconds), "seconds it shrinks to nothing at the end of its life", 10.0f),
        smoke_field("blind_seconds", offsetof(SmokeTuning, blind_seconds),
            "seconds the enemies it pops on can't see or act", 30.0f),
        smoke_field("noise_radius", offsetof(SmokeTuning, noise_radius), "metres: the pop is a loud noise this far", 50.0f),
        smoke_field("throw_release_seconds", offsetof(SmokeTuning, throw_release_seconds),
            "seconds from the start of the throw until the bomb leaves the hand (the cloud pops then)", 2.0f),
        smoke_field("throw_recovery_seconds", offsetof(SmokeTuning, throw_recovery_seconds),
            "seconds after the release until the player acts again (the Throw clip's follow-through is cut)", 2.0f),
    }, default_smoke_tuning());

    g_types.execution_tuning = register_data_asset_type("execution_tuning", {
        ranged(field("execution_range", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_range),
            "metres: an enemy is executed from this close (flat)"), 0.0f, 10.0f),
        ranged(field("execution_behind_half_angle_degrees", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_behind_half_angle_degrees),
            "degrees off its back direction the player can be: half the arc at its back"), 0.0f, 180.0f),
        ranged(field("execution_lunge_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_lunge_seconds),
            "seconds of the lunge to the spot behind the enemy"), 0.0f, 2.0f),
        ranged(field("execution_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_seconds),
            "seconds of the whole execution, lunge included"), 0.0f, 5.0f),
        ranged(field("execution_hit_stop_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_hit_stop_seconds),
            "the freeze when the strike lands"), 0.0f, 1.0f),
        ranged(field("execution_damage", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_damage),
            "damage of an execution on an enemy whose type it doesn't kill"), 0.0f, 1000.0f),
        ranged(field("execution_noise_radius", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, execution_noise_radius),
            "metres: enemies this close hear the kill (like footsteps, not loud)"), 0.0f, 30.0f),
        ranged(field("aerial_min_drop", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, aerial_min_drop),
            "metres above the ground the player must be for an aerial assassination"), 0.0f, 20.0f),
        ranged(field("aerial_aim_radius", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, aerial_aim_radius),
            "metres around the predicted landing point the plunge finds its target"), 0.0f, 20.0f),
        ranged(field("plunge_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, plunge_speed),
            "metres per second of the plunge onto the target"), 0.0f, 100.0f),
        ranged(field("knockdown_radius_per_metre", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, knockdown_radius_per_metre),
            "metres of knockdown radius around the plunge's target per metre of drop"), 0.0f, 10.0f),
        ranged(field("knockdown_max_radius", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, knockdown_max_radius),
            "metres: the knockdown radius is never larger"), 0.0f, 20.0f),
        ranged(field("knockdown_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, knockdown_seconds),
            "seconds a knocked down enemy lies"), 0.0f, 10.0f),
        ranged(field("chain_window_seconds", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, chain_window_seconds),
            "seconds after an execution or a plunge in which attack continues the chain"), 0.0f, 10.0f),
        ranged(field("chain_range", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, chain_range),
            "metres: the chain reaches the next enemy this close"), 0.0f, 30.0f),
        ranged(field("chain_dash_speed", ENGINE_FIELD_KIND_FLOAT, offsetof(ExecutionTuning, chain_dash_speed),
            "metres per second of the dash to the next enemy of the chain"), 0.0f, 100.0f),
    }, default_execution_tuning());

    // version 2: the tuning moved to the data assets
    g_types.enemy_config = register_component("enemy_config", {
        data_asset("type", offsetof(EnemyConfig, type), "enemy_type", "the kind of enemy: its tuning, shared by the enemies of the kind (assets/data/enemies)"),
    }, default_enemy_config(), 2);

    g_types.player_config = register_component("player_config", {
        data_asset("tuning", offsetof(PlayerConfig, tuning), "player_tuning", "the player's tuning (assets/data/player)"),
        data_asset("traversal", offsetof(PlayerConfig, traversal), "traversal_tuning", "how the player climbs and vaults (assets/data/player)"),
        data_asset("executions", offsetof(PlayerConfig, executions), "execution_tuning", "how the player executes enemies (assets/data/player)"),
        data_asset("momentum", offsetof(PlayerConfig, momentum), "momentum_tuning", "what fills and drains the player's momentum (assets/data/player)"),
        data_asset("smoke", offsetof(PlayerConfig, smoke), "smoke_tuning", "the player's smoke bomb (assets/data/player)"),
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

    g_types.enemy_spawner = register_component("enemy_spawner", {
        data_asset("enemy_type", offsetof(EnemySpawnerConfig, enemy_type), "enemy_type", "the kind of enemies it spawns (assets/data/enemies)"),
        ranged(field("count", ENGINE_FIELD_KIND_UINT32, offsetof(EnemySpawnerConfig, count), "how many enemies it spawns"), 0.0f, 64.0f),
        ranged(field("area_half_extents", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemySpawnerConfig, area_half_extents),
            "metres: the area is [-x, x] x [-z, z] around the spawner, in its space", 2), 0.0f, 50.0f),
        ranged(field("min_spacing", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemySpawnerConfig, min_spacing), "metres between two enemies, at least"), 0.0f, 10.0f),
        field("seed", ENGINE_FIELD_KIND_UINT32, offsetof(EnemySpawnerConfig, seed), "the same seed places the same enemies"),
    }, default_enemy_spawner());
}

const project_c::GameComponentTypes& project_c::game_components()
{
    return g_types;
}
