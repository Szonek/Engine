#pragma once


#include "engine.h"
#include "engine_vector.h"
#include "iscene.h"
#include "../nav_mesh.h"

#include <cstdint>

namespace project_c
{
struct UI_data
{
    engine_ui_document_t doc;
    engine_ui_data_handle_t handle_test;
    engine_ui_data_handle_t handle_items_on_ground;
    std::uint32_t character_health = 100;
    std::uint32_t enemy_health = 100;

    engine_vector_uint32_t items_on_ground = engineVectorCreateUint32();
    
    engine_game_object_t item_go = ENGINE_INVALID_GAME_OBJECT_ID;
    engine_string_t item_name = engineStringCreate("sword");
    engine_string_t item_pos_x = engineStringCreate("50%");
    engine_string_t item_pos_y = engineStringCreate("75%");
    bool show_item = false;
};

class TestScene : public engine::IScene
{
public:
    TestScene(engine::IApplication* app);
    ~TestScene();

    void activate()
    {
        IScene::activate();
        if (ui_data_.doc)
        {
            engineUiDocumentShow(ui_data_.doc);
        }
    }

    void deactivate()
    {
        IScene::deactivate();
        if (ui_data_.doc)
        {
            engineUiDocumentHide(ui_data_.doc);
        }
    }

    void update_hook_begin() override;
    static constexpr const char* get_name() { return "TestScene"; }

    void ui_update_item_on_ground(const class Sword* sw);
    void ui_remove_item_from_ground(const class Sword* sw);
private:
    UI_data ui_data_;
    NavMesh nav_mesh_;
};

}// namespace project_c