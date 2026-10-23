#pragma once
#include <engine.h>

#include <cstdint>

// The designer-set values of the game's scripts: game-defined components (plain C structs the engine describes field
// by field), saved in scene files and edited in the editor. Scripts read them each time they use a value, so an edit
// while playing takes effect at once; what changes while playing stays in the scripts' members and is never saved.
namespace project_c
{
struct EnemyConfig
{
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
};

struct PlayerConfig
{
    std::uint32_t max_hp;
    // per hit on an enemy (each dagger)
    std::uint32_t attack_damage;
    // metres per second
    float move_speed;
    float dodge_speed;
    float dodge_cooldown_seconds;
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

// the handles of the game's component types, ENGINE_INVALID_OBJECT_HANDLE until register_game_components()
struct GameComponentTypes
{
    engine_component_type_t enemy_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t player_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t building_part_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t house_config = ENGINE_INVALID_OBJECT_HANDLE;
    engine_component_type_t player_start = ENGINE_INVALID_OBJECT_HANDLE;
};

// Registers the game's component types with the active application (once, at startup, before scene files are loaded);
// a type which fails is logged and stays ENGINE_INVALID_OBJECT_HANDLE.
void register_game_components();
const GameComponentTypes& game_components();

// the default values, what a new component of the type holds
EnemyConfig default_enemy_config();
PlayerConfig default_player_config();
BuildingPartConfig default_building_part_config();
HouseConfig default_house_config();
PlayerStart default_player_start();

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
}  // namespace project_c
