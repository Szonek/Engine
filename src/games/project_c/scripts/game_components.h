#pragma once
#include <engine.h>

#include <cstdint>

// The designer-set values of the game's scripts: game-defined components (plain C structs the engine describes field
// by field), saved in scene files and edited in the editor, and the data assets they reference (a value shared by every
// object referencing it, in a file under assets/data, like Unity's ScriptableObject). Scripts read them each time they
// use a value, so an edit while playing (in the editor, or of a data asset's file) takes effect at once; what changes
// while playing stays in the scripts' members and is never saved.
namespace project_c
{
inline constexpr std::uint32_t ENEMY_TYPE_MAX_ATTACKS = 4;
inline constexpr std::uint32_t ANIMATION_NAME_MAX_LENGTH = 40;

// One attack of an enemy type: an animation of its model, and when in it the blade deals damage. The damage comes from
// the blade's hitbox, so the window is tuned per animation: the part of it where the blade sweeps the area in front of
// the enemy.
struct EnemyAttack
{
    // "" for none
    char animation[ANIMATION_NAME_MAX_LENGTH];
    // normalized animation time the blade starts / stops dealing damage
    float active_start;
    float active_end;
};

// the tuning of a kind of enemy, shared by the enemies of the kind (a data asset, assets/data/enemies)
struct EnemyType
{
    // the model the enemies of the kind are made of (spawners and LevelBuilder::enemy instantiate it)
    engine_prefab_t prefab;
    std::uint32_t max_hp;
    // per hit on the player
    std::uint32_t attack_damage;
    // metres per second
    float move_speed;
    // metres: attacks start this close to the player
    float attack_range;
    float attack_cooldown_seconds;
    // the colour the enemy flashes when hit
    float hit_flash_color[3];
    // used in turn, in this order; the ones without an animation are skipped
    EnemyAttack attacks[ENEMY_TYPE_MAX_ATTACKS];
};

struct EnemyConfig
{
    // an EnemyType data asset; with none, the enemy has default_enemy_type()
    engine_data_asset_t type;
};

// the player's tuning (a data asset, assets/data/player)
struct PlayerTuning
{
    std::uint32_t max_hp;
    // per hit on an enemy (each dagger)
    std::uint32_t attack_damage;
    // metres per second
    float move_speed;
    float dodge_speed;
    float dodge_cooldown_seconds;
    float dodge_invulnerability_seconds;
    // multiplies the attack animations' speed
    float attack_playback_rate;
    // an attack pressed this long before the current one ends is the combo's next one
    float attack_input_buffer_seconds;
    float attack_combo_cross_fade_seconds;
    float attack_exit_cross_fade_seconds;
    float turn_rate_degrees_per_second;
    // metres per second per second, toward the wanted speed while moving / toward stopping
    float movement_acceleration;
    float movement_braking;
    // the freeze on a hit which lands
    float hit_stop_seconds;
    float hit_camera_shake_intensity;
    float hit_camera_shake_seconds;
    // being hit
    float hit_stun_seconds;
    float knockback_speed;
};

struct PlayerConfig
{
    // a PlayerTuning data asset; with none, the player has default_player_tuning()
    engine_data_asset_t tuning;
};

// what the House hides while the player is inside it
enum BuildingPartHide : std::int32_t
{
    BUILDING_PART_HIDE_NEVER = 0,
    BUILDING_PART_HIDE_MESH = 1,
    // the collider goes too, so mouse picking goes through it (the roof)
    BUILDING_PART_HIDE_MESH_AND_COLLIDER = 2,
};

struct BuildingPartConfig
{
    // the player can climb its colliders
    bool climbable;
    // only from outside of its parent (the house), i.e. the faces pointing away from the parent's position
    bool climbable_from_outside_only;
    // traversal aligns to the part's position (the center of a window opening)
    bool align_to_position;
    BuildingPartHide hide_when_inside;
};

inline constexpr std::uint32_t HOUSE_MAX_ROOF_PROPS = 8;

struct HouseConfig
{
    // game objects standing on the roof, hidden together with it
    engine_game_object_t roof_props[HOUSE_MAX_ROOF_PROPS];
};

inline constexpr std::uint32_t PLAYER_START_ID_MAX_LENGTH = 32;

// Where the player enters a level (like Unreal's PlayerStart): levels hold player starts, never the player, which the
// level spawns at one when it is loaded (LevelBuilder::spawn_player). It has no script.
struct PlayerStart
{
    // the entrance's name, for the level exit leading here; "default" is where a level starts
    char id[PLAYER_START_ID_MAX_LENGTH];
};

// Places `count` enemies of a type at random points of its area when it starts (EnemySpawner, like Unreal's spawn volumes
// or a Diablo-style monster pack): the level holds the spawner, never its enemies, which are runtime objects. The same
// seed places the same enemies on every platform.
struct EnemySpawnerConfig
{
    // an EnemyType data asset; with none, nothing is spawned
    engine_data_asset_t enemy_type;
    std::uint32_t count;
    // metres: the area is [-x, x] x [-z, z] around the spawner, in its space (it turns with it)
    float area_half_extents[2];
    // metres between two enemies, at least
    float min_spacing;
    std::uint32_t seed;
};

inline constexpr std::uint32_t LEVEL_PATH_MAX_LENGTH = 128;

// Where a level exit leads (the LevelExit script)
struct LevelExitConfig
{
    // the level file, relative to assets/scenes (or absolute)
    char level[LEVEL_PATH_MAX_LENGTH];
    // the id of the player start there where the player enters
    char entrance[PLAYER_START_ID_MAX_LENGTH];
};

// the handles of the game's component and data asset types, ENGINE_INVALID_OBJECT_HANDLE until
// register_game_components()
struct GameComponentTypes
{
    engine_component_type_t enemy_attack = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t enemy_type = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t player_tuning = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t enemy_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t player_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t building_part_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t house_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t player_start = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t level_exit = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t enemy_spawner = ENGINE_INVALID_OBJECT_HANDLE;
};

// Registers the game's component and data asset types with the active application (once, at startup, before scene
// files and data assets are loaded); a type which fails is logged and stays ENGINE_INVALID_OBJECT_HANDLE.
void register_game_components();
const GameComponentTypes& game_components();

// the default values: what a new component of the type holds, what a data asset holds in the fields its file doesn't
// have
EnemyType default_enemy_type();
PlayerTuning default_player_tuning();
EnemyConfig default_enemy_config();
PlayerConfig default_player_config();
BuildingPartConfig default_building_part_config();
HouseConfig default_house_config();
PlayerStart default_player_start();
LevelExitConfig default_level_exit();
EnemySpawnerConfig default_enemy_spawner();

// The game object's component of the type, or `fallback` when it has none (a script placed without its config).
template<typename T>
T get_game_component(engine_game_object_t go, engine_component_type_t type, const T& fallback)
{
    T value = fallback;
    if (engineGameObjectGetDataComponent(go, type, &value) != ENGINE_RESULT_CODE_OK)
    {
        return fallback;
    }
    return value;
}

// The data asset's value of the type, or `fallback` when it holds none (no asset, or one of another type).
template<typename T>
T get_data_asset(engine_data_asset_t asset, engine_component_type_t type, const T& fallback)
{
    T value = fallback;
    if (engineDataAssetGet(asset, type, &value) != ENGINE_RESULT_CODE_OK)
    {
        return fallback;
    }
    return value;
}
}  // namespace project_c
