#include "enemy_spawner_script.h"

#include "enemy_script.h"
#include "../app.h"
#include "../random.h"
#include "../scenes/level_builder.h"

#include <iscene.h>
#include <utils.h>

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdint>

namespace
{
// random points tried per enemy before the spawner gives up (the area is too small for the spacing, or mostly off the
// nav mesh)
constexpr std::uint32_t TRIES_PER_ENEMY = 30;
// how far from a random point the nav mesh is searched for the point an enemy stands on
constexpr float NAV_SEARCH_HALF_EXTENTS[3] = { 0.5f, 2.0f, 0.5f };
}  // namespace

project_c::EnemySpawner::EnemySpawner(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

project_c::EnemySpawnerConfig project_c::EnemySpawner::config() const
{
    return get_game_component(go_, game_components().enemy_spawner, default_enemy_spawner());
}

void project_c::EnemySpawner::on_start()
{
    const auto cfg = config();
    if (cfg.count == 0)
    {
        return;
    }
    if (cfg.enemy_type == ENGINE_INVALID_OBJECT_HANDLE)
    {
        log("An enemy spawner without an enemy type spawns nothing.\n");
        return;
    }
    // where the spawner is in the world now, not as of the last scene update: it starts right after the level loaded
    float world[16] = {};
    if (engineGameObjectComputeWorldTransform(go_, world) != ENGINE_RESULT_CODE_OK)
    {
        log("An enemy spawner without a transform spawns nothing.\n");
        return;
    }
    const auto matrix = glm::make_mat4(world);

    Random random(cfg.seed);
    LevelBuilder builder(*static_cast<AppProjectC*>(my_scene_->get_app()), *my_scene_);
    const float half_x = std::max(0.0f, cfg.area_half_extents[0]);
    const float half_z = std::max(0.0f, cfg.area_half_extents[1]);
    std::vector<glm::vec3> placed;
    for (std::uint32_t tries = 0; placed.size() < cfg.count && tries < cfg.count * TRIES_PER_ENEMY; tries++)
    {
        const glm::vec3 local(random.range(-half_x, half_x), 0.0f, random.range(-half_z, half_z));
        const float yaw_degrees = random.range(0.0f, 360.0f);
        const glm::vec3 point(matrix * glm::vec4(local, 1.0f));
        glm::vec3 on_nav_mesh{};
        if (engineNavFindNearestPoint(glm::value_ptr(point), NAV_SEARCH_HALF_EXTENTS, glm::value_ptr(on_nav_mesh)) != ENGINE_RESULT_CODE_OK)
        {
            continue;
        }
        const bool too_close = std::any_of(placed.begin(), placed.end(), [&](const glm::vec3& other) {
            return glm::distance(glm::vec2(other.x, other.z), glm::vec2(on_nav_mesh.x, on_nav_mesh.z)) < cfg.min_spacing;
        });
        if (too_close)
        {
            continue;
        }
        auto* enemy = builder.spawn_enemy(cfg.enemy_type, on_nav_mesh, yaw_degrees);
        if (!enemy)
        {
            // the type has no prefab (logged): the others would fail the same way
            break;
        }
        placed.push_back(on_nav_mesh);
        spawned_.push_back(enemy->get_game_object());
    }
    if (placed.size() < cfg.count)
    {
        log(fmt::format("An enemy spawner placed {} of its {} enemies: its area has no room for more, {} m apart, on the nav mesh.\n",
            placed.size(), cfg.count, cfg.min_spacing));
    }
}
