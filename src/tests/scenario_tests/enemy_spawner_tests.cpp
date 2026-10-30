#include "sandbox_scene.h"

#include <app.h>
#include <scripts/encounter_script.h>
#include <scripts/enemy_script.h>
#include <scripts/enemy_spawner_script.h>
#include <scripts/game_components.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

// The enemy spawner: its enemies are placed from its seed, on the nav mesh, and count for the encounter.
namespace
{
using project_c::Encounter;
using project_c::EnemySpawnerConfig;

class EnemySpawnerTest : public scenario::GameplayTest
{
protected:
    EnemySpawnerConfig spawner_config(std::uint32_t seed) const
    {
        auto config = project_c::default_enemy_spawner();
        config.enemy_type = scenario::app().get_skeleton_warrior_type();
        config.count = 5;
        config.area_half_extents[0] = 3.0f;
        config.area_half_extents[1] = 2.0f;
        config.min_spacing = 0.8f;
        config.seed = seed;
        return config;
    }

    // a sandbox (scene_) with a floor and a spawner, after its first frame (the spawner spawns when it starts)
    project_c::EnemySpawner* spawn(const EnemySpawnerConfig& config)
    {
        scene_ = load_sandbox_scene();
        scene_->build().floor_area(-5.0f, -5.0f, 5.0f, 5.0f);
        auto* spawner = scene_->build().enemy_spawner(0.5f, 0.0f, config);
        step(1);
        return spawner;
    }

    // where a spawner with the config places its enemies, in a sandbox of its own
    std::vector<glm::vec3> spawned_positions(const EnemySpawnerConfig& config)
    {
        const auto positions = spawned_positions(*spawn(config));
        TearDown();
        return positions;
    }

    std::vector<glm::vec3> spawned_positions(const project_c::EnemySpawner& spawner) const
    {
        std::vector<glm::vec3> positions;
        for (const auto go : spawner.get_spawned())
        {
            positions.push_back(position_of(go));
        }
        return positions;
    }

    scenario::SandboxScene* scene_ = nullptr;
};

TEST_F(EnemySpawnerTest, SameSeedPlacesTheSameEnemies)
{
    const auto first = spawned_positions(spawner_config(7));
    const auto again = spawned_positions(spawner_config(7));
    ASSERT_EQ(first.size(), 5u);
    ASSERT_EQ(again.size(), first.size());
    for (std::size_t i = 0; i < first.size(); i++)
    {
        EXPECT_EQ(first[i], again[i]) << "enemy " << i;
    }

    const auto other_seed = spawned_positions(spawner_config(8));
    ASSERT_EQ(other_seed.size(), first.size());
    EXPECT_NE(other_seed, first);
}

TEST_F(EnemySpawnerTest, PlacesItsCountOfRuntimeEnemiesInItsAreaOnTheNavMesh)
{
    const auto config = spawner_config(3);
    const auto* spawner = spawn(config);
    ASSERT_EQ(spawner->get_spawned().size(), config.count);
    EXPECT_EQ(scene_->get_scripts<project_c::Enemy>().size(), config.count);

    std::vector<glm::vec3> placed;
    for (const auto go : spawner->get_spawned())
    {
        // runtime objects: the level file holds the spawner, never its enemies
        EXPECT_TRUE(engineGameObjectIsTransient(go));
        const auto position = position_of(go);
        // in the area around the spawner at (0.5, 0)
        EXPECT_LE(std::abs(position.x - 0.5f), config.area_half_extents[0] + 0.01f);
        EXPECT_LE(std::abs(position.z), config.area_half_extents[1] + 0.01f);
        // standing on the nav mesh
        constexpr float extents[3] = { 0.05f, 1.0f, 0.05f };
        const float point[3] = { position.x, position.y, position.z };
        float on_nav_mesh[3] = {};
        ASSERT_EQ(engineNavFindNearestPoint(point, extents, on_nav_mesh), ENGINE_RESULT_CODE_OK);
        EXPECT_NEAR(on_nav_mesh[0], position.x, 0.01f);
        EXPECT_NEAR(on_nav_mesh[2], position.z, 0.01f);
        for (const auto& other : placed)
        {
            EXPECT_GE(glm::distance(glm::vec2(other.x, other.z), glm::vec2(position.x, position.z)), config.min_spacing);
        }
        placed.push_back(position);
    }
}

TEST_F(EnemySpawnerTest, AreaTooSmallForTheSpacingPlacesFewer)
{
    auto config = spawner_config(1);
    config.area_half_extents[0] = 0.5f;
    config.area_half_extents[1] = 0.5f;
    config.min_spacing = 2.0f;
    const auto* spawner = spawn(config);
    // the area's corners are less than 2 m apart: one enemy fits
    EXPECT_EQ(spawner->get_spawned().size(), 1u);
}

TEST_F(EnemySpawnerTest, KillingTheSpawnedEnemiesClearsTheEncounter)
{
    const auto* spawner = spawn(spawner_config(5));
    step(1);
    EXPECT_EQ(scene_->encounter()->get_state(), Encounter::State::eInProgress);
    EXPECT_EQ(scene_->encounter()->get_remaining_enemies(), spawner->get_spawned().size());

    for (auto* enemy : scene_->get_scripts<project_c::Enemy>())
    {
        enemy->receive_attack_hit(enemy->get_max_hp(), 0.0f);
    }
    step(2);
    EXPECT_EQ(scene_->encounter()->get_state(), Encounter::State::eCleared);
}
}  // namespace
