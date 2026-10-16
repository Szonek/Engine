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

#include "../nav_mesh.h"

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

    void spawn(EnemyPack& pack, std::int32_t count, const Point& world_pos, const SpawnAreaRect& area, const project_c::NavMesh& nav_mesh, project_c::AppProjectC& app, engine::IScene& scene)
    {
        for (std::int32_t i = 0; i < count; i++)
        {
            const auto enemy_idx = 0;// dist_(rng_) % pack.infos.size();
            const auto offset_x = std::uniform_real_distribution<float>(area.x_min, area.x_max)(rng_);
            const auto offset_y = std::uniform_real_distribution<float>(area.x_min, area.x_max)(rng_);
            scene.register_script<project_c::Enemy>(app.instantiate_prefab(pack.types[enemy_idx], &scene), &nav_mesh, world_pos.x + offset_x, world_pos.z + offset_y);
        }
    }

private:
    std::mt19937 rng_;
    std::vector<project_c::Enemy*> mobs_;
};


inline void generate_scene(std::string_view scene_str, project_c::NavMesh& nav_mesh, project_c::AppProjectC& app, engine::IScene& scene)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::mt19937::result_type> dist6(0, 1);

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

    std::vector<std::vector<project_c::NavMeshNodeIdx>> nodes_id;
    nodes_id.resize(scene_width);
    for (auto& row : nodes_id)
    {
        row.resize(scene_height, project_c::invalid_node_idx);
    }
    for (std::size_t x = 0; x < scene_width; x++)
    {
        for (std::size_t z = 0; z < scene_height; z++)
        {
            const auto c = scene_str[z * (scene_width + 1) + x];  // + 1 because of '\n' in every line
            const auto x_offset = (float)std::int32_t(x - scene_width / 2);
            const auto z_offset = (float)std::int32_t(z - scene_height / 2);
            if (c == '|' || c == '-')
            {             
                if (app.is_prefab_available(project_c::PREFAB_TYPE_WALL))
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
                    scene.register_script<project_c::Wall>(app.instantiate_prefab(project_c::PREFAB_TYPE_WALL, &scene).go, 
                        x_offset + x_offset_snap, z_offset + z_offset_snap, y_rotation);
                }
                if (app.is_prefab_available(project_c::PREFAB_FLOOR_OUTSIDE_REGION))
                {
                    auto sc = scene.register_script<project_c::FloorOutsideRegion>(app.instantiate_prefab(project_c::PREFAB_FLOOR_OUTSIDE_REGION, &scene).go);
                    sc->set_world_position(x_offset, 0.0f, z_offset);
                }
            }
            else if (c == 'x')
            {
                if (app.is_prefab_available(project_c::PREFAB_FLOOR_OUTSIDE_REGION))
                {
                    auto sc = scene.register_script<project_c::FloorOutsideRegion>(app.instantiate_prefab(project_c::PREFAB_FLOOR_OUTSIDE_REGION, &scene).go);
                    sc->set_world_position(x_offset, 0.0f, z_offset);
                }
            }
            else if (c == 'h' || c == 'B')
            {
                // house footprint: floor without nav mesh node, enemies do not walk into (or under) the house
                if (app.is_prefab_available(project_c::PREFAB_TYPE_FLOOR))
                {
                    scene.register_script<project_c::Floor>(app.instantiate_prefab(project_c::PREFAB_TYPE_FLOOR, &scene).go, x_offset, z_offset);
                }
                scene_spawn_points.house_cells.push_back({ x_offset, z_offset });
                if (c == 'B')
                {
                    scene_spawn_points.house_chests.push_back({ x_offset, z_offset });
                }
            }
            else
            {
                if (app.is_prefab_available(project_c::PREFAB_TYPE_FLOOR))
                {
                    scene.register_script<project_c::Floor>(app.instantiate_prefab(project_c::PREFAB_TYPE_FLOOR, &scene).go, x_offset, z_offset);
                    const auto id = nav_mesh.add_node({ x_offset, 0.0f, z_offset }, { 0.5f, 0.0f, 0.5f });
                    nodes_id[x][z] = id;
                }
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
    // construct edges based on vector if ids
    for (auto x = 0; x < scene_width; x++)
    {
        for (auto z = 0; z < scene_height; z++)
        {
            const auto id = nodes_id[x][z];
            if (id == project_c::invalid_node_idx)
            {
                continue;
            }
            // vertical and horizontal edges
            if (x > 0)
            {
                const auto left_id = nodes_id[x - 1][z];
                if (left_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, left_id, 1.0f);
                }
            }

            if (x < scene_width - 1)
            {
                const auto right_id = nodes_id[x + 1][z];
                if (right_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, right_id, 1.0f);
                }
            }

            if (z > 0)
            {
                const auto up_id = nodes_id[x][z - 1];
                if (up_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, up_id, 1.0f);
                }
            }

            if (z < scene_height - 1)
            {
                const auto down_id = nodes_id[x][z + 1];
                if (down_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, down_id, 1.0f);
                }
            }

            // diagonal edges
            if (x > 0 && z > 0)
            {
                const auto left_up_id = nodes_id[x - 1][z - 1];
                if (left_up_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, left_up_id, 1.414f);
                }
            }

            if (x < scene_width - 1 && z > 0)
            {
                const auto right_up_id = nodes_id[x + 1][z - 1];
                if (right_up_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, right_up_id, 1.414f);
                }
            }

            if (x > 0 && z < scene_height - 1)
            {
                const auto left_down_id = nodes_id[x - 1][z + 1];
                if (left_down_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, left_down_id, 1.414f);
                }
            }

            if (x < scene_width - 1 && z < scene_height - 1)
            {
                const auto right_down_id = nodes_id[x + 1][z + 1];
                if (right_down_id != project_c::invalid_node_idx)
                {
                    nav_mesh.add_edge(id, right_down_id, 1.414f);
                }
            }
        }
    }
    // at this point nav mesh has to be completed!
    if (app.is_prefab_available(project_c::PREFAB_TYPE_ROGUE))
    {
        for (const auto& point : scene_spawn_points.player)
        {
            auto s = scene.register_script<project_c::Player>(app.instantiate_prefab(project_c::PREFAB_TYPE_ROGUE, &scene));
            s->set_world_position(point.x, 1.0f, point.y);
            auto w = scene.register_script<project_c::Weapon>();
            const auto item_equipped = s->equip_waepon(w);
        }
    }
    if (app.is_prefab_available(project_c::PREFAB_TYPE_SKELETON_WARRIOR))
    {
        for (const auto& point : scene_spawn_points.enemy_packs)
        {
            EnemyPack pack{ {project_c::PrefabType::PREFAB_TYPE_SKELETON_WARRIOR} };
            MobPackSpawner spawner;
            const auto spawn_area = MobPackSpawner::SpawnAreaRect{ -1.0f, 1.0f, -1.0f, 1.0f };
            //const auto spawn_area = MobPackSpawner::SpawnAreaRect{ 0.0f, 0.0f, 0.0f, 0.0f };
            const auto spawn_world_pos = MobPackSpawner::Point{ point.x, point.y };
            spawner.spawn(pack, 5, spawn_world_pos, spawn_area, nav_mesh, app, scene);
        }
    }

    for (const auto& point : scene_spawn_points.point_lights)
    {
        auto l = scene.register_script<project_c::PointLight>();
        l->set_world_position(point.x, 1.0f, point.y);
    }

    for (const auto& chest : scene_spawn_points.chests)
    {
        auto c = scene.register_script<project_c::Chest>(app.instantiate_prefab(project_c::PREFAB_TYPE_BOX, &scene).go);
        c->set_world_position(chest.x, 0.1f, chest.y);
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
        auto house = scene.register_script<project_c::House>(app, (min_cell + max_cell) * 0.5f, FLOOR_TOP_Y);
        if (app.is_prefab_available(project_c::PREFAB_TYPE_BOX))
        {
            // same height offset relative to the ground as chests placed on the floor
            const auto roof = house->get_roof_center();
            auto c = scene.register_script<project_c::Chest>(app.instantiate_prefab(project_c::PREFAB_TYPE_BOX, &scene).go);
            c->set_world_position(roof.x + 0.5f, roof.y - (FLOOR_TOP_Y - 0.1f), roof.z - 0.5f);
            house->add_roof_prop(c->get_game_object());

            for (const auto& chest : scene_spawn_points.house_chests)
            {
                auto hc = scene.register_script<project_c::Chest>(app.instantiate_prefab(project_c::PREFAB_TYPE_BOX, &scene).go);
                hc->set_world_position(chest.x, 0.1f, chest.y);
            }
        }
    }

    // weapons reuse geometry and texture of the rogue asset, so check it here
    if (app.is_prefab_available(project_c::PREFAB_TYPE_ROGUE))
    {
        for (const auto& wpn : scene_spawn_points.weapons)
        {
            auto w = scene.register_script<project_c::Weapon>();
            w->drop_on_ground(glm::vec3(wpn.x, 0.5f, wpn.y));
        }
    }

}
}


project_c::TestScene::TestScene(engine::IApplication* app)
    : IScene(app)
{
    auto camera_script = register_script<CameraScript>();
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
    generate_scene(scene_str, nav_mesh_, *typed_app, *this);
    camera_script->snap_to_player();
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
