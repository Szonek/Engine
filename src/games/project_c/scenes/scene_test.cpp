#include "scene_test.h"

#include "../app.h"
#include "../scripts.h"

namespace
{
inline void generate_floor(std::int32_t map_border_distance_x, std::int32_t map_border_distance_z, project_c::AppProjectC& app, engine::IScene& scene)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::mt19937::result_type> dist6(0, 1);
    for (std::int32_t x = -map_border_distance_x; x <= map_border_distance_x; x++)
    {
        for (std::int32_t z = -map_border_distance_z; z <= map_border_distance_z; z++)
        {
            if (x == -map_border_distance_x || x == map_border_distance_x || z == -map_border_distance_z || z == map_border_distance_z)
            {
                scene.register_script<project_c::Wall>(app.instantiate_prefab(project_c::PREFAB_TYPE_WALL, &scene), x, z);
            }
            else
            {
                auto flor_moodel = dist6(rng) ? project_c::PREFAB_TYPE_FLOOR_DETAIL : project_c::PREFAB_TYPE_FLOOR;
                scene.register_script<project_c::Floor>(app.instantiate_prefab(flor_moodel, &scene), x, z);
            }

        }
    }
};

}

project_c::TestScene::TestScene(engine::IApplication* app)
    : IScene(app)
{
    auto app_handle = app->get_handle();
    auto camera_script = register_script<CameraScript>();

    if (engineApplicationAddFontFromFile(app_handle, "tahoma.ttf", "tahoma_font") != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return;
    }
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

    auto typed_app = dynamic_cast<AppProjectC*>(app);
    register_script<project_c::Solider>(typed_app->instantiate_prefab(project_c::PREFAB_TYPE_SOLIDER, this));
    register_script<MainLight>();
    register_script<PointLight>();
    generate_floor(9, 3, *typed_app, *this);
}