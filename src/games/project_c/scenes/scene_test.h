#pragma once


#include "engine.h"
#include "engine_string.h"
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
        engine_vector_uint32_t progressbar_value = engineVectorCreateUint32();  // in range: <0; 100>
    };
    engine_vector_uint32_t go = engineVectorCreateUint32();
    healthbar healhbars{};
};

struct UI_data
{
    engine_ui_document_t doc;
    engine_ui_data_handle_t handle_main_ui;

    std::uint32_t player_health = 100;
    std::uint32_t player_max_health = 100;
    engine_string_t* encounter_status = nullptr;
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
            gameplay_ui_visible_ ? engineUiDocumentShow(ui_data_.doc) : engineUiDocumentHide(ui_data_.doc);
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

    // the level's per-frame rules on top of IScene::update; runs also while the editor is paused
    engine_result_code_t update(float dt) override;
    void update_hook_begin() override;
    static constexpr const char* get_name() { return "TestScene"; }

    void ui_update_item_on_ground(const class Weapon* sw);
    void ui_remove_item_from_ground(const class Weapon* sw) const;

    void ui_update_enemy(const class Enemy* en);
    void ui_remove_enemy(const class Enemy* en);
    void ui_update_player_health(std::uint32_t health);
    void on_enemy_spawned();
    void on_enemy_defeated();
    void on_player_died();
    void set_gameplay_ui_visible(bool visible);
    bool is_encounter_over() const { return encounter_over_; }
private:
    UI_data ui_data_{};
    NavMesh nav_mesh_{};
    std::uint32_t remaining_enemies_ = 0;
    bool encounter_over_ = false;
    bool gameplay_started_ = false;
    bool gameplay_ui_visible_ = true;
    bool restart_was_down_ = false;
};

}// namespace project_c