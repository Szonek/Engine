#pragma once
#include <engine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string_view>

namespace engine
{
class IScene;
}

namespace project_c
{
class AppProjectC;
class CameraScript;
class Chest;
class Encounter;
class Enemy;
class EnemySpawner;
class GameplayHud;
class House;
class LevelExit;
class Player;
class Weapon;
struct EnemySpawnerConfig;

// Places level content into a scene the way the game does: the camera, the light, the nav surface, floor tiles, walls,
// the player, player starts, level exits, enemies, chests, weapons, the house, and the Encounter and GameplayHud scripts. Each object gets its
// components (name, transform, colliders, its config component with the default values) and, when it has behaviour,
// its script as data (IScene::attach_script: a `script` component naming it), so the scene is what a scene file of it
// would load. Levels made in code (the scenario tests' scenes) build with it; LevelScene uses it to spawn the player. The nav mesh is built from the
// static colliders with engineNavMeshBuild() once everything is placed.
// x/z are world positions in metres; floor tiles sit on the 1 m grid. Objects whose prefab failed to load are skipped
// (nullptr / no-op), like the level always did.
class LevelBuilder
{
public:
    LevelBuilder(AppProjectC& app, engine::IScene& scene);

    // the orthographic top-down camera following the player
    CameraScript* camera();
    // the directional light (with shadows) of the level
    engine_game_object_t main_light();
    // the level's win/lose rules
    Encounter* encounter();
    // the gameplay UI
    GameplayHud* hud();

    // the game object with the scene's nav surface component (nav mesh settings for the enemies)
    engine_game_object_t nav_surface();
    // floor tile at cell (x, z)
    LevelBuilder& floor(float x, float z);
    // walkable floor for every cell in [x_min, x_max] x [z_min, z_max]
    LevelBuilder& floor_area(float x_min, float z_min, float x_max, float z_max);
    // floor tile left out of the nav mesh: enemies never walk there (e.g. inside the house)
    LevelBuilder& floor_not_walkable(float x, float z);
    // the dark ground outside the playable area
    LevelBuilder& floor_outside_region(float x, float z);
    // the Wall prefab (currently not loaded, see default_prefabs_data())
    LevelBuilder& wall(float x, float z, float y_rotation_degrees);

    // invisible static box collider standing on the floor (box center at `center`, full `size` in metres).
    // It's not a BuildingPart, so the player bumps into it and never climbs it.
    engine_game_object_t obstacle(const glm::vec3& center, const glm::vec3& size);
    // the player; it equips its starting weapon itself when it starts
    Player* player(float x, float z);
    // The player as a runtime object (transient, never saved) at the player start's world position, facing where it
    // faces: what a level does when it is loaded (levels hold player starts, not the player). nullptr when the start
    // has no transform or the player's prefab isn't loaded.
    Player* spawn_player(engine_game_object_t player_start);
    // Where the player enters the level from the level exit naming `id` ("default": where the level starts), above
    // the floor at (x, z) facing `yaw_degrees` around +y (0: +z). Place it a few steps away from the level's own
    // exits, so the player arriving there doesn't stand in one.
    engine_game_object_t player_start(float x, float z, float yaw_degrees, std::string_view id);
    // A 1 x 1 m pad on the floor centered on (x, z): walking onto it takes the player to `level` (relative to
    // assets/scenes), entering it at the player start `entrance`.
    LevelExit* level_exit(float x, float z, std::string_view level, std::string_view entrance);
    // a skeleton warrior (the level's enemy type)
    Enemy* enemy(float x, float z);
    // An enemy of the type as a runtime object (transient, never saved) standing at `position`, facing `yaw_degrees`
    // around +y (0: +z): what an EnemySpawner spawns. nullptr when the type has no prefab.
    Enemy* spawn_enemy(engine_data_asset_t type, const glm::vec3& position, float yaw_degrees);
    // spawns the enemies `config` describes around (x, z) on the floor when the level starts
    EnemySpawner* enemy_spawner(float x, float z, const EnemySpawnerConfig& config);
    Chest* chest(float x, float y, float z);
    Weapon* weapon_on_ground(float x, float z);
    // a house centered on (x, z) standing on ground_y, with its walls and roof (see build_house)
    House* house(float x, float z, float ground_y);

private:
    // a floor tile, or ENGINE_INVALID_GAME_OBJECT_ID when its prefab isn't loaded
    engine_game_object_t place_floor(float x, float z);
    // an instance of the enemy type's prefab with its name and enemy_config, or ENGINE_INVALID_GAME_OBJECT_ID when the
    // type has no prefab (logged)
    engine_game_object_t instantiate_enemy(engine_data_asset_t type, bool transient);
    // what the player is made of, on an instance of its prefab
    Player* set_up_player(engine_game_object_t go, const glm::vec3& position, const glm::quat& rotation);

    AppProjectC& app_;
    engine::IScene& scene_;
};
}  // namespace project_c
