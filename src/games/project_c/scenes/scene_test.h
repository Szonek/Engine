#pragma once


#include "engine.h"
#include "engine_vector.h"
#include "iscene.h"
#include "../nav_mesh.h"

#include <cstdint>

namespace project_c
{
struct UI_data_items_on_ground
{
    engine_vector_uint32_t go = engineVectorCreateUint32();
    engine_vector_engine_string_t name = engineVectorCreateEngineString();
    engine_vector_engine_string_t pos_x = engineVectorCreateEngineString();
    engine_vector_engine_string_t pos_y = engineVectorCreateEngineString();
    engine_vector_uint32_t show = engineVectorCreateUint32();
};

struct UI_data_enemy
{
    struct healthbar
    {
        engine_vector_engine_string_t pos_x = engineVectorCreateEngineString();
        engine_vector_engine_string_t pos_y = engineVectorCreateEngineString();
        engine_vector_uint32_t show = engineVectorCreateUint32();
    };
    engine_vector_uint32_t go = engineVectorCreateUint32();
    healthbar healhbars{};
};

struct UI_data
{
    engine_ui_document_t doc;
    engine_ui_data_handle_t handle_main_ui;

    UI_data_enemy enemies{};
    UI_data_items_on_ground items{};
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

    void ui_update_enemy(const class Enemy* en);
    void ui_remove_enemy(const class Enemy* en);
private:
    UI_data ui_data_{};
    NavMesh nav_mesh_{};
};

}// namespace project_c