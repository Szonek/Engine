#pragma once


#include "engine.h"
#include "iscene.h"

#include <cstdint>

namespace project_c
{
struct UI_data
{
    engine_ui_document_t doc;
    engine_ui_data_handle_t handle;
    std::uint32_t character_health = 100;
    std::uint32_t enemy_health = 100;
};

class TestScene : public engine::IScene
{
public:
    TestScene(engine::IApplication* app);

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

    void update_hook_begin() override
    {
        engineUiDataHandleDirtyVariable(ui_data_.handle, "character_health");
        engineUiDataHandleDirtyVariable(ui_data_.handle, "enemy_health");
    }

    ~TestScene()
    {
        engineUiDataHandleDestroy(ui_data_.handle);
        engineApplicationUiDocumentDestroy(ui_data_.doc);
    }
    static constexpr const char* get_name() { return "TestScene"; }

    UI_data& get_ui_data() { return ui_data_; }
    const UI_data& get_ui_data() const { return ui_data_; }
private:
    UI_data ui_data_;
};

}// namespace project_c