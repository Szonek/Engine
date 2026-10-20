#pragma once
#include <engine.h>

#include <glm/glm.hpp>

namespace engine
{
class IScene;
}

namespace project_c
{
class AppProjectC;
class Chest;
class Enemy;
class Player;
class Weapon;

// Places gameplay objects into a scene the way the game does: the nav surface, floor tiles, walls, the player, enemies,
// chests and weapons. The level (TestScene's ASCII map) and the scenario tests both build with it. The nav mesh is built
// from the static colliders with engineNavMeshBuild() once everything is placed.
// x/z are world positions in metres; floor tiles sit on the 1 m grid. Objects whose prefab failed to load are skipped
// (nullptr / no-op), like the level always did.
class LevelBuilder
{
public:
    LevelBuilder(AppProjectC& app, engine::IScene& scene);

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
    // the player, with a weapon equipped
    Player* player(float x, float z);
    Enemy* enemy(float x, float z);
    Chest* chest(float x, float y, float z);
    Weapon* weapon_on_ground(float x, float z);

private:
    AppProjectC& app_;
    engine::IScene& scene_;
};
}  // namespace project_c
