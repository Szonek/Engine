#pragma once
#include "../nav_mesh.h"

#include <engine.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <map>
#include <utility>

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

// Places gameplay objects into a scene the way the game does: floor tiles with nav mesh nodes, walls, the player,
// enemies, chests and weapons. The level (TestScene's ASCII map) and the scenario tests both build with it.
// x/z are world positions in metres; floor tiles sit on the 1 m grid. Objects whose prefab failed to load are skipped
// (nullptr / no-op), like the level always did.
class LevelBuilder
{
public:
    LevelBuilder(AppProjectC& app, engine::IScene& scene, NavMesh& nav_mesh);

    // floor tile at cell (x, z) with a nav mesh node, connected to the nodes of the 8 neighbouring cells
    LevelBuilder& floor(float x, float z);
    // walkable floor for every cell in [x_min, x_max] x [z_min, z_max]
    LevelBuilder& floor_area(float x_min, float z_min, float x_max, float z_max);
    // floor tile without a nav mesh node: enemies never walk there (e.g. under the house)
    LevelBuilder& floor_without_nav(float x, float z);
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
    NavMesh& nav_mesh_;
    std::map<std::pair<std::int32_t, std::int32_t>, NavMeshNodeIdx> nav_nodes_;
};
}  // namespace project_c
