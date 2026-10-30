#pragma once
#include "base_script.h"
#include "game_components.h"

#include <vector>

namespace project_c
{
// Spawns the enemies its enemy_spawner component describes when it starts, at random points of its area snapped onto
// the nav mesh, at least the minimum spacing apart (like Unreal's spawn volumes or a Diablo-style monster pack). Level
// content: the level holds the spawner (LevelBuilder::enemy_spawner), its enemies are runtime objects, never saved. The
// same seed places the same enemies, so a restart gives the same level. Needs the level's nav mesh, built before the
// first update.
class EnemySpawner : public BaseNode
{
public:
    EnemySpawner(engine::IScene* my_scene, engine_game_object_t go);

    // its designer-set values (the enemy_spawner component)
    EnemySpawnerConfig config() const;

    void on_start() override;

    // the enemies it spawned (they can be destroyed since)
    const std::vector<engine_game_object_t>& get_spawned() const { return spawned_; }

private:
    std::vector<engine_game_object_t> spawned_;
};
}  // namespace project_c
