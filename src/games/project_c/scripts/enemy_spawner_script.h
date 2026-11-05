#pragma once
#include "base_script.h"
#include "game_components.h"

#include <glm/glm.hpp>

#include <limits>
#include <vector>

namespace project_c
{
// Spawns the enemies its enemy_spawner component describes when it starts, at random points of its area snapped onto
// the nav mesh, at least the minimum spacing apart (like Unreal's spawn volumes or a Diablo-style monster pack). Level
// content: the level holds the spawner (LevelBuilder::enemy_spawner), its enemies are runtime objects, never saved. The
// same seed places the same enemies, so the level is the same every time it is played. Needs the level's nav mesh, built before the
// first update. Its enemies are a pack: they alert each other (pack_state, which each member reads).
class EnemySpawner : public BaseNode
{
public:
    EnemySpawner(engine::IScene* my_scene, engine_game_object_t go);

    // its designer-set values (the enemy_spawner component)
    EnemySpawnerConfig config() const;

    void on_start() override;

    // what the pack knows of the player
    struct PackState
    {
        // a member is alert
        bool alert = false;
        // where the member which perceived the player last noticed it
        glm::vec3 last_known_position{ 0.0f };
        // since a member perceived the player itself (infinite when none did)
        float seconds_since_perceived = std::numeric_limits<float>::infinity();
    };
    // What the members other than `reader` know of the player, from their awareness now: a member's own perception never
    // keeps it alert through its pack, so a pack member forgets like a placed enemy. A dying member is left out, but
    // its killing blow alerts the pack (Enemy::was_just_killed).
    PackState pack_state(engine_game_object_t reader) const;

    // the enemies it spawned (they can be destroyed since)
    const std::vector<engine_game_object_t>& get_spawned() const { return spawned_; }

private:
    std::vector<engine_game_object_t> spawned_;
};
}  // namespace project_c
