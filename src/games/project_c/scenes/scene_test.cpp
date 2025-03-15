#include "scene_test.h"

#include "../app.h"
#include "../scripts/camera_script.h"
#include "../scripts/enviorment_script.h"
#include "../scripts/enemy_script.h"
#include "../scripts/solider_script.h"
#include "../scripts/scripts_utils.h"

#include "../nav_mesh.h"

#include <engine_vector.h>

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
        std::vector<engine_coords_2d_t> solider2;
        std::vector<engine_coords_2d_t> enemy_packs;
        std::vector<engine_coords_2d_t> point_lights;
        std::vector<engine_coords_2d_t> weapons;
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
                if (app.is_prefab_available(project_c::PREFAB_TYPE_WALL))
                {
                    scene.register_script<project_c::Wall>(app.instantiate_prefab(project_c::PREFAB_TYPE_WALL, &scene).go, x_offset, z_offset);
                }
            }
            else
            {
                if (app.is_prefab_available(project_c::PREFAB_TYPE_FLOOR) || app.is_prefab_available(project_c::PREFAB_TYPE_FLOOR_DETAIL))
                {
                    auto flor_moodel = dist6(rng) ? project_c::PREFAB_TYPE_FLOOR_DETAIL : project_c::PREFAB_TYPE_FLOOR;
                    scene.register_script<project_c::Floor>(app.instantiate_prefab(flor_moodel, &scene).go, x_offset, z_offset);
                    const auto id = nav_mesh.add_node({ x_offset, 0.0f, z_offset }, { 0.5f, 0.0f, 0.5f });
                    nodes_id[x][z] = id;
                }
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
            else if (c == 'w')
            {
                scene_spawn_points.weapons.push_back({ x_offset, z_offset });
            }
            else if (c == 'l')
            {
                scene_spawn_points.solider2.push_back({ x_offset, z_offset });
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
    if (app.is_prefab_available(project_c::PREFAB_TYPE_SOLIDER))
    {
        for (const auto& point : scene_spawn_points.solider)
        {
             auto s = scene.register_script<project_c::Solider>(app.instantiate_prefab(project_c::PREFAB_TYPE_SOLIDER, &scene));
            s->set_world_position(point.x, 0.0f, point.y);
        }
    }

    if (app.is_prefab_available(project_c::PREFAB_TYPE_SOLIDER2))
    {
        for (const auto& point : scene_spawn_points.solider2)
        {
            auto s = scene.register_script<project_c::Solider2>(app.instantiate_prefab(project_c::PREFAB_TYPE_SOLIDER2, &scene));
            //s->set_world_position(point.x, 0.0f, point.y);
        }
    }

    if (app.is_prefab_available(project_c::PREFAB_TYPE_ORC))
    {
        for (const auto& point : scene_spawn_points.enemy_packs)
        {
            EnemyPack pack{ {project_c::PrefabType::PREFAB_TYPE_ORC} };
            MobPackSpawner spawner;
            const auto spawn_area = MobPackSpawner::SpawnAreaRect{ -1.0f, 1.0f, -1.0f, 1.0f };
            //const auto spawn_area = MobPackSpawner::SpawnAreaRect{ 0.0f, 0.0f, 0.0f, 0.0f };
            const auto spawn_world_pos = MobPackSpawner::Point{ point.x, point.y };
            spawner.spawn(pack, 1, spawn_world_pos, spawn_area, nav_mesh, app, scene);
        }
    }

    for (const auto& point : scene_spawn_points.point_lights)
    {
        auto l = scene.register_script<project_c::PointLight>();
        l->set_world_position(point.x, 1.0f, point.y);
    }

    if (app.is_prefab_available(project_c::PREFAB_TYPE_SWORD))
    {
        for (const auto& wpn : scene_spawn_points.weapons)
        {
            auto w = scene.register_script<project_c::Sword>(app.instantiate_prefab(project_c::PREFAB_TYPE_SWORD, &scene).go);
            w->drop_on_ground(glm::vec3(wpn.x, 0.5f, wpn.y));
        }
    }
}
}


void equip_sword_callback(engine_ui_data_handle_t data_handle, const engine_ui_event_t* ev, const engine_vector_engine_ui_data_variant_t args, void* user_data)
{
    assert(args != nullptr);
    assert(engineVectorSizeEngineUiDataVariant(args) == 1);
    const auto arg_0 = engineVectorGetEngineUiDataVariant(args, 0);
    assert(arg_0.type == ENGINE_UI_DATA_TYPE_UINT32);
    const auto item_go = arg_0.arg.u32;

    auto scene = reinterpret_cast<project_c::TestScene*>(user_data);
    auto solider_go = project_c::utils::get_game_objects_with_name(scene->get_handle(), "solider");
    assert(solider_go.size() == 1);
    auto solider_script = scene->get_script<project_c::Solider>(solider_go[0]);
    const auto item_equipped = solider_script->equip_sword(scene->get_script<project_c::Sword>(item_go));
    if (!item_equipped)
    {
        engineLog(std::format("Tried to equip item, but couldnt do so!\n").c_str());
    }
}

inline void register_ui_item_bindings(std::vector<engine_ui_document_data_binding_t>& registry, project_c::UI_data& ui_data, project_c::TestScene* scene)
{
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "items_go";
        binding.data_vector_uint32_t = ui_data.items.go;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_UINT32;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "items_name";
        binding.data_vector_string = ui_data.items.name;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_STRING;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "items_pos_x";
        binding.data_vector_string = ui_data.items.pos_x;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_STRING;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "items_pos_y";
        binding.data_vector_string = ui_data.items.pos_y;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_STRING;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "items_show";
        binding.data_vector_uint32_t = ui_data.items.show;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_UINT32;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "equip";
        binding.data_callback.fn_ptr = &equip_sword_callback;
        binding.data_callback.user_data = scene;
        binding.type = ENGINE_UI_DATA_TYPE_EVENT_CALLBACK;
        registry.push_back(binding);
    }
}

inline void register_ui_enemy_bindings(std::vector<engine_ui_document_data_binding_t>& registry, project_c::UI_data& ui_data)
{
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "enemies_go";
        binding.data_vector_uint32_t = ui_data.enemies.go;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_UINT32;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "enemies_health";
        binding.data_vector_uint32_t = ui_data.enemies.healhbars.progressbar_value;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_UINT32;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "enemies_pos_x";
        binding.data_vector_string = ui_data.enemies.healhbars.pos_x;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_STRING;
        registry.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "enemies_pos_y";
        binding.data_vector_string = ui_data.enemies.healhbars.pos_y;
        binding.type = ENGINE_UI_DATA_TYPE_VECTOR_STRING;
        registry.push_back(binding);
    }
}

project_c::TestScene::TestScene(engine::IApplication* app)
    : IScene(app)
{
    auto app_handle = app->get_handle();
    auto camera_script = register_script<CameraScript>();

    std::vector<engine_ui_document_data_binding_t> bindings {};
    register_ui_item_bindings(bindings, ui_data_, this);
    register_ui_enemy_bindings(bindings, ui_data_);

    engineApplicationCreateUiDocumentDataHandle(app_handle, "DataModel_Main_UI", bindings.data(), bindings.size(), &ui_data_.handle_main_ui);

    // load ui doc
    engineApplicationCreateUiDocumentFromFile(app_handle, "project_c_gameplay_ui.rml", &ui_data_.doc);
    if (ui_data_.doc)
    {
        engineUiDocumentShow(ui_data_.doc);
    }

    const std::string scene_str =
        //"xxxxxxxxxxx\n"
        //"x         x\n"
        //"x         x\n"
        //"x         x\n"
        //"x         x\n"
        //"x     x   x\n"
        //"xxxxxxxxxxx\n"
        //"x    p    x\n"
        "x         x\n"
        "x  w  ee  x\n"
        "xsl   ee  x\n"
        "x     ee  x\n"
        "x         x\n";
        //"xxxxxxxxxxx\n";
    register_script<MainLight>();
    auto typed_app = static_cast<AppProjectC*>(app);
    generate_scene(scene_str, nav_mesh_, *typed_app, *this);

}

project_c::TestScene::~TestScene()
{
    engineUiDataHandleDestroy(ui_data_.handle_main_ui);
    engineApplicationUiDocumentDestroy(ui_data_.doc);
}

void project_c::TestScene::update_hook_begin()
{
    //engineUiDataHandleDirtyVariable(ui_data_.handle_test, "character_health");
    //engineUiDataHandleDirtyVariable(ui_data_.handle_test, "enemy_health");
}

void project_c::TestScene::ui_update_item_on_ground(const project_c::Sword* sw)
{
    const auto active_camera_go = utils::get_active_camera_game_objects(scene_)[0];
    const auto item_go = sw->get_game_object();
    const auto item_tc = engineSceneGetTransformComponent(scene_, item_go);
    const auto item_screen_coords = engineSceneCameraComponentConvertWorldPositionToScreenPosition(scene_, active_camera_go, item_tc.position);
    
    const auto x_str = std::to_string(item_screen_coords.x * 100) + "%";
    const auto y_str = std::to_string(item_screen_coords.y * 100) + "%";

    bool found = false;
    for (auto i = 0; i < engineVectorSizeUint32(ui_data_.items.go); i++)
    {
        const auto go = engineVectorGetUint32(ui_data_.items.go, i);
        if (go == item_go)
        {
            found = true;
            engineVectorSetUint32(ui_data_.items.show, i, 1);

            auto pos_x_str = engineVectorGetEngineString(ui_data_.items.pos_x, i);
            engineStringSet(pos_x_str, x_str.c_str());

            auto pos_y_str = engineVectorGetEngineString(ui_data_.items.pos_y, i);
            engineStringSet(pos_y_str, y_str.c_str());
            break;
        }
    }
    if (!found)
    {
        engineVectorPushBackUint32(ui_data_.items.go, item_go);
        engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_go");
        engineVectorPushBackEngineString(ui_data_.items.name, engineStringCreate("sword"));
        engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_name");

        engineVectorPushBackUint32(ui_data_.items.show, 1);
        engineVectorPushBackEngineString(ui_data_.items.pos_x, engineStringCreate(x_str.c_str()));
        engineVectorPushBackEngineString(ui_data_.items.pos_y, engineStringCreate(y_str.c_str()));
    }

    engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_show");
    engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_pos_x");
    engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_pos_y");
}

void project_c::TestScene::ui_remove_item_from_ground(const project_c::Sword* sw)
{
    for (auto i = 0; i < engineVectorSizeUint32(ui_data_.items.go); i++)
    {
        const auto go = engineVectorGetUint32(ui_data_.items.go, i);
        if (go == sw->get_game_object())
        {
            engineVectorEraseUint32(ui_data_.items.go, i);
            engineVectorEraseUint32(ui_data_.items.show, i);
            engineVectorEraseEngineString(ui_data_.items.name, i);
            engineVectorEraseEngineString(ui_data_.items.pos_x, i);
            engineVectorEraseEngineString(ui_data_.items.pos_y, i);

            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_go");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_show");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_name");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_pos_x");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "items_pos_y");

            return;
        }
    }
}

void project_c::TestScene::ui_update_enemy(const Enemy* en)
{
    const auto active_camera_go = utils::get_active_camera_game_objects(scene_)[0];
    const auto enemy_go = en->get_game_object();
    auto enemy_tc = engineSceneGetTransformComponent(scene_, enemy_go);
    auto healthbar_position = enemy_tc.position;
    const auto height_offset = 1.0f; // healthbar need to be on top of the enemy
    healthbar_position[1] += height_offset;
    const auto enemy_screen_coords = engineSceneCameraComponentConvertWorldPositionToScreenPosition(scene_, active_camera_go, healthbar_position);

    const auto box_width = 10; // percent, ToDo: get propery from UiElement
    const auto box_height = 1; // percent, ToDo: get propery from UiElement

    // calculate x and y and we need to center the box, so subtract half of the size of the box
    const auto x_str = std::to_string(enemy_screen_coords.x * 100 - (box_width /2)) + "%";
    const auto y_str = std::to_string(enemy_screen_coords.y * 100 - (box_height /2)) + "%";

    // calculate health progress bar
    const auto health_progress_value = static_cast<std::uint32_t>(((float)en->hp / (float)en->max_hp) * 100);

    bool found = false;
    for (auto i = 0; i < engineVectorSizeUint32(ui_data_.enemies.go); i++)
    {
        const auto go = engineVectorGetUint32(ui_data_.enemies.go, i);
        if (go == enemy_go)
        {
            found = true;

            engineVectorSetUint32(ui_data_.enemies.healhbars.progressbar_value, i, health_progress_value);

            auto pos_x_str = engineVectorGetEngineString(ui_data_.enemies.healhbars.pos_x, i);
            engineStringSet(pos_x_str, x_str.c_str());

            auto pos_y_str = engineVectorGetEngineString(ui_data_.enemies.healhbars.pos_y, i);
            engineStringSet(pos_y_str, y_str.c_str());
            break;
        }
    }
    if (!found)
    {
        engineVectorPushBackUint32(ui_data_.enemies.go, enemy_go);
        engineVectorPushBackUint32(ui_data_.enemies.healhbars.progressbar_value, health_progress_value);
        engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_go");

        engineVectorPushBackEngineString(ui_data_.enemies.healhbars.pos_x, engineStringCreate(x_str.c_str()));
        engineVectorPushBackEngineString(ui_data_.enemies.healhbars.pos_y, engineStringCreate(y_str.c_str()));
    }

    engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_health");
    engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_pos_x");
    engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_pos_y");
}

void project_c::TestScene::ui_remove_enemy(const Enemy* en)
{
    for (auto i = 0; i < engineVectorSizeUint32(ui_data_.enemies.go); i++)
    {
        const auto go = engineVectorGetUint32(ui_data_.enemies.go, i);
        if (go == en->get_game_object())
        {
            engineVectorEraseUint32(ui_data_.enemies.go, i);
            engineVectorEraseUint32(ui_data_.enemies.healhbars.progressbar_value, i);
            engineVectorEraseEngineString(ui_data_.enemies.healhbars.pos_x, i);
            engineVectorEraseEngineString(ui_data_.enemies.healhbars.pos_y, i);

            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_go");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_health");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_pos_x");
            engineUiDataHandleDirtyVariable(ui_data_.handle_main_ui, "enemies_pos_y");

            return;
        }
    }
}
