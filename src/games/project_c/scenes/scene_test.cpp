#include "scene_test.h"

#include "../app.h"
#include "../scripts.h"

project_c::TestScene::TestScene(engine::IApplication* app)
    : IScene(app)
{
    auto app_handle = app->get_handle();
    auto camera_script = register_script<CameraScript>();

    auto typed_app = dynamic_cast<AppProjectC*>(app);
    typed_app->instantiate_prefab<project_c::Solider>(project_c::PREFAB_TYPE_SOLIDER, this);

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

}