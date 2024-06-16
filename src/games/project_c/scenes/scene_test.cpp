#include "scene_test.h"

#include "../app.h"
#include "../scripts/camera_script.h"
#include "../scripts/enviorment_script.h"
#include "../scripts/enemy_script.h"
#include "../scripts/solider_script.h"

#include "../nav_mesh.h"

#include <random>
#include <chrono>

namespace
{
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
        std::vector<engine_coords_2d_t> solider;
        std::vector<engine_coords_2d_t> enemy_packs;
        std::vector<engine_coords_2d_t> point_lights;
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
            if (c == 'x')
            {
                scene.register_script<project_c::Wall>(app.instantiate_prefab(project_c::PREFAB_TYPE_WALL, &scene).go, x_offset, z_offset);
            }
            else
            {
                auto flor_moodel = dist6(rng) ? project_c::PREFAB_TYPE_FLOOR_DETAIL : project_c::PREFAB_TYPE_FLOOR;
                scene.register_script<project_c::Floor>(app.instantiate_prefab(flor_moodel, &scene).go, x_offset, z_offset);
                const auto id = nav_mesh.add_node({ x_offset, 0.0f, z_offset }, { 0.5f, 0.0f, 0.5f });
                nodes_id[x][z] = id;
            }

            if(c =='s')
            {
                scene_spawn_points.solider.push_back({ x_offset, z_offset });
            }
            else if (c == 'e')
            {
                scene_spawn_points.enemy_packs.push_back({ x_offset, z_offset });
            }
            else if (c == 'p')
            {
                scene_spawn_points.point_lights.push_back({ x_offset, z_offset });
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

    for (const auto& point : scene_spawn_points.solider)
    {
        auto s = scene.register_script<project_c::Solider>(app.instantiate_prefab(project_c::PREFAB_TYPE_SOLIDER, &scene));
        s->set_world_position(point.x, 0.0f, point.y);
    }

    for (const auto& point : scene_spawn_points.enemy_packs)
    {
        EnemyPack pack{ {project_c::PrefabType::PREFAB_TYPE_ORC} };
        MobPackSpawner spawner;
        const auto spawn_area = MobPackSpawner::SpawnAreaRect{ -1.0f, 1.0f, -1.0f, 1.0f };
        //const auto spawn_area = MobPackSpawner::SpawnAreaRect{ 0.0f, 0.0f, 0.0f, 0.0f };
        const auto spawn_world_pos = MobPackSpawner::Point{ point.x, point.y };
        spawner.spawn(pack, 1, spawn_world_pos, spawn_area, nav_mesh, app, scene);
    }

    for (const auto& point : scene_spawn_points.point_lights)
    {
        auto l = scene.register_script<project_c::PointLight>();
        l->set_world_position(point.x, 1.0f, point.y);
    }
}


}

project_c::TestScene::TestScene(engine::IApplication* app)
    : IScene(app)
{
    auto app_handle = app->get_handle();
    auto camera_script = register_script<CameraScript>();

    std::array<engine_ui_document_data_binding_t, 2> bindings{};
    bindings[0].data_uint32_t = &ui_data_.character_health;
    bindings[0].name = "character_health";
    bindings[0].type = ENGINE_DATA_TYPE_UINT32;

    bindings[1].data_uint32_t = &ui_data_.enemy_health;
    bindings[1].name = "enemy_health";
    bindings[1].type = ENGINE_DATA_TYPE_UINT32;

    engineApplicationCreateUiDocumentDataHandle(app_handle, "health", bindings.data(), bindings.size(), &ui_data_.handle);

    // load ui doc
    engineApplicationCreateUiDocumentFromFile(app_handle, "project_c_health_bar.rml", &ui_data_.doc);
    if (ui_data_.doc)
    {
        engineUiDocumentShow(ui_data_.doc);
    }

    const std::string scene_str = 
        "xxxxxxxxxxx\n"
        "x         x\n"
        "x         x\n"
        "x         x\n"
        "x         x\n"
        "x     x   x\n"
        "xxxxxxxx  x\n"
        "xs   p    x\n"
        "x         x\n"
        "x    e    x\n"
        "x         x\n"
        "x         x\n"
        "xxxxxxxxxxx\n";

    auto typed_app = static_cast<AppProjectC*>(app);
    generate_scene(scene_str, nav_mesh_, *typed_app, *this);
}