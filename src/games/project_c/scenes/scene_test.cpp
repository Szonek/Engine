#include "scene_test.h"

#include "../app.h"
#include "../scripts/camera_script.h"
#include "../scripts/enviorment_script.h"
#include "../scripts/enemy_script.h"
#include "../scripts/player_script.h"
#include "../scripts/interactable_script.h"
#include "../scripts/building_script.h"
#include "../scripts/encounter_script.h"
#include "../scripts/gameplay_hud_script.h"
#include "../scripts/scripts_utils.h"
#include "level_builder.h"

#include <engine_vector.h>

#include <limits>
#include <random>
#include <chrono>

namespace
{
// top of the Floor_Prototype tile collider
constexpr float FLOOR_TOP_Y = 0.125f;

struct EnemyPack
{
    std::vector<project_c::PrefabType> types;
};
class MobPackSpawner
{
public:
    // delete copy ctor, default move ctor
    MobPackSpawner()
        : rng_(std::random_device()())
    {
    }
    MobPackSpawner(const MobPackSpawner&) = delete;
    MobPackSpawner(MobPackSpawner&&) = default;
    MobPackSpawner& operator=(const MobPackSpawner&) = delete;
    MobPackSpawner& operator=(MobPackSpawner&&) = default;
    virtual ~MobPackSpawner() = default;

    struct Point
    {
        float x;
        float z;
    };

    struct SpawnAreaRect
    {
        float x_min;
        float x_max;
        float z_min;
        float z_max;
    };

    void spawn(EnemyPack& pack, std::int32_t count, const Point& world_pos, const SpawnAreaRect& area, project_c::LevelBuilder& builder)
    {
        for (std::int32_t i = 0; i < count; i++)
        {
            const auto offset_x = std::uniform_real_distribution<float>(area.x_min, area.x_max)(rng_);
            const auto offset_y = std::uniform_real_distribution<float>(area.x_min, area.x_max)(rng_);
            builder.enemy(world_pos.x + offset_x, world_pos.z + offset_y);
        }
    }

private:
    std::mt19937 rng_;
};


inline void generate_scene(std::string_view scene_str, project_c::AppProjectC& app, engine::IScene& scene)
{
    project_c::LevelBuilder builder(app, scene);
    builder.nav_surface();

    struct SceneSpawnPoints
    {
        std::vector<engine_fvec2_t> player;
        std::vector<engine_fvec2_t> enemy_packs;
        std::vector<engine_fvec2_t> point_lights;
        std::vector<engine_fvec2_t> weapons;
        std::vector<engine_fvec2_t> chests;
        std::vector<engine_fvec2_t> house_cells;
        std::vector<engine_fvec2_t> house_chests;
    } scene_spawn_points;

    const auto scene_width = (std::int32_t)scene_str.find_first_of('\n');
    const auto scene_height = std::count(scene_str.begin(), scene_str.end(), '\n');

    for (std::size_t x = 0; x < scene_width; x++)
    {
        for (std::size_t z = 0; z < scene_height; z++)
        {
            const auto c = scene_str[z * (scene_width + 1) + x];  // + 1 because of '\n' in every line
            const auto x_offset = (float)std::int32_t(x - scene_width / 2);
            const auto z_offset = (float)std::int32_t(z - scene_height / 2);
            if (c == '|' || c == '-')
            {
                auto x_offset_snap = 0.0f;
                auto z_offset_snap = 0.0f;
                auto y_rotation = 0.0f;
                if (c == '|')
                {
                    y_rotation = 90.0f; // rotate wall by 90 degrees
                    const auto c_left = scene_str[z * (scene_width + 1) + x - 1];
                    const auto c_right = scene_str[z * (scene_width + 1) + x + 1];
                    // add offset to snap to the grid
                    if (c_left == 'x' || c_right != 'x')
                    {
                        x_offset_snap += 0.5f;
                    }
                    else if (c_right == 'x' || c_left != 'x')
                    {
                        x_offset_snap -= 0.5f;
                    }
                }

                else if (c == '-')
                {
                    // add offset to snap to the grid
                    if (z_offset > 0)
                    {
                        z_offset_snap -= 0.5f;
                    }
                    else if (z_offset < 0)
                    {
                        z_offset_snap += 0.5f;
                    }
                }
                builder.wall(x_offset + x_offset_snap, z_offset + z_offset_snap, y_rotation);
                builder.floor_outside_region(x_offset, z_offset);
            }
            else if (c == 'x')
            {
                builder.floor_outside_region(x_offset, z_offset);
            }
            else if (c == 'h' || c == 'B')
            {
                // house footprint: enemies do not walk into the house
                builder.floor_not_walkable(x_offset, z_offset);
                scene_spawn_points.house_cells.push_back({ x_offset, z_offset });
                if (c == 'B')
                {
                    scene_spawn_points.house_chests.push_back({ x_offset, z_offset });
                }
            }
            else
            {
                builder.floor(x_offset, z_offset);
            }

            if(c =='p')
            {
                scene_spawn_points.player.push_back({ x_offset, z_offset });
            }
            else if (c == 'e')
            {
                scene_spawn_points.enemy_packs.push_back({ x_offset, z_offset });
            }
            else if (c == 'p')
            {
                scene_spawn_points.point_lights.push_back({ x_offset, z_offset });
            }
            else if (c == 'w')
            {
                scene_spawn_points.weapons.push_back({ x_offset, z_offset });
            }
            else if (c == 'b')
            {
                scene_spawn_points.chests.push_back({ x_offset, z_offset });
            }
        }
    }

    for (const auto& point : scene_spawn_points.player)
    {
        builder.player(point.x, point.y);
    }
    for (const auto& point : scene_spawn_points.enemy_packs)
    {
        EnemyPack pack{ {project_c::PrefabType::PREFAB_TYPE_SKELETON_WARRIOR} };
        MobPackSpawner spawner;
        const auto spawn_area = MobPackSpawner::SpawnAreaRect{ -1.0f, 1.0f, -1.0f, 1.0f };
        const auto spawn_world_pos = MobPackSpawner::Point{ point.x, point.y };
        spawner.spawn(pack, 5, spawn_world_pos, spawn_area, builder);
    }

    for (const auto& point : scene_spawn_points.point_lights)
    {
        auto l = scene.register_script<project_c::PointLight>();
        l->set_world_position(point.x, 1.0f, point.y);
    }

    for (const auto& chest : scene_spawn_points.chests)
    {
        builder.chest(chest.x, 0.1f, chest.y);
    }

    // house built over the 'h' footprint (centered on its bounding box), with a chest on the roof as a reward for climbing
    if (!scene_spawn_points.house_cells.empty())
    {
        glm::vec2 min_cell(std::numeric_limits<float>::max());
        glm::vec2 max_cell(std::numeric_limits<float>::lowest());
        for (const auto& cell : scene_spawn_points.house_cells)
        {
            min_cell = glm::min(min_cell, glm::vec2(cell.x, cell.y));
            max_cell = glm::max(max_cell, glm::vec2(cell.x, cell.y));
        }
        const auto center = (min_cell + max_cell) * 0.5f;
        auto* house = builder.house(center.x, center.y, FLOOR_TOP_Y);
        // same height offset relative to the ground as chests placed on the floor
        const auto roof = house->get_roof_center();
        if (auto* c = builder.chest(roof.x + 0.5f, roof.y - (FLOOR_TOP_Y - 0.1f), roof.z - 0.5f))
        {
            house->add_roof_prop(c->get_game_object());

            for (const auto& chest : scene_spawn_points.house_chests)
            {
                builder.chest(chest.x, 0.1f, chest.y);
            }
        }
    }

    for (const auto& wpn : scene_spawn_points.weapons)
    {
        builder.weapon_on_ground(wpn.x, wpn.y);
    }

    if (engineNavMeshBuild() != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to build the level's nav mesh; enemies will not move.\n");
    }
}
}


project_c::TestScene::TestScene(engine::IApplication* app)
    : IScene(app)
{
    register_script<CameraScript>();
    register_script<Encounter>();
    hud_ = register_script<GameplayHud>();

    // h - house footprint (house is built over it), B - chest inside the house
    const std::string scene_str =
        "                     \n"
        "                     \n"
        "                     \n"
        "        hhhhh        \n"
        "        hhhBh        \n"
        "        hhhhh        \n"
        "        hhhhh        \n"
        "        hhhhh        \n"
        "                     \n"
        "                  e  \n"
        "        p            \n"
        "                     \n"
        "                     \n";


    register_script<MainLight>();
    auto typed_app = static_cast<AppProjectC*>(app);
    generate_scene(scene_str, *typed_app, *this);
}

void project_c::TestScene::activate()
{
    IScene::activate();
    if (hud_)
    {
        hud_->set_visible(engineEditorIsGameView());
    }
}

void project_c::TestScene::deactivate()
{
    IScene::deactivate();
    if (hud_)
    {
        hud_->set_visible(false);
    }
}

engine_result_code_t project_c::TestScene::update(float dt)
{
    const auto result = IScene::update(dt);

    // the gameplay UI follows the editor's game/editor view (always the game view without the editor)
    hud_->set_visible(engineEditorIsGameView());

    // once the encounter is over (cleared or player dead), restart starts the level again
    auto* app = static_cast<AppProjectC*>(get_app());
    const bool restart_down = app->player_input().restart;
    const bool restart_pressed = restart_down && !restart_was_down_;
    restart_was_down_ = restart_down;
    const auto* encounter = find_script<Encounter>();
    if (encounter && encounter->is_over() && restart_pressed)
    {
        app->request_scene_reload<TestScene>();
    }
    return result;
}
