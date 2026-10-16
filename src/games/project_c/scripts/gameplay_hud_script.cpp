#include "gameplay_hud_script.h"
#include "enemy_script.h"
#include "player_script.h"
#include "scripts_utils.h"

#include <iapplication.h>
#include <iscene.h>

#include <format>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
void equip_weapon_callback(engine_ui_data_handle_t data_handle, const engine_ui_event_t* ev, const engine_vector_engine_ui_data_variant_t args, void* user_data)
{
    assert(args != nullptr);
    assert(engineVectorSizeEngineUiDataVariant(args) == 1);
    const auto arg_0 = engineVectorGetEngineUiDataVariant(args, 0);
    assert(arg_0.type == ENGINE_UI_DATA_TYPE_UINT32);
    const auto item_go = arg_0.arg.u32;

    auto scene = reinterpret_cast<engine::IScene*>(user_data);
    auto player_script = scene->find_script<project_c::Player>();
    const auto item_equipped = player_script && player_script->equip_waepon(scene->get_script<project_c::Weapon>(item_go));
    if (!item_equipped)
    {
        engineLog(std::format("Tried to equip item, but couldnt do so!\n").c_str());
    }
}

void add_binding(std::vector<engine_ui_document_data_binding_t>& registry, const char* name, engine_vector_uint32_t data)
{
    engine_ui_document_data_binding_t binding = {};
    binding.name = name;
    binding.data_vector_uint32_t = data;
    binding.type = ENGINE_UI_DATA_TYPE_VECTOR_UINT32;
    registry.push_back(binding);
}

void add_binding(std::vector<engine_ui_document_data_binding_t>& registry, const char* name, engine_vector_engine_string_t data)
{
    engine_ui_document_data_binding_t binding = {};
    binding.name = name;
    binding.data_vector_string = data;
    binding.type = ENGINE_UI_DATA_TYPE_VECTOR_STRING;
    registry.push_back(binding);
}
}  // namespace

project_c::GameplayHud::GameplayHud(engine::IScene* my_scene)
    : BaseNode(my_scene, "gameplay-hud")
{
    auto* app = my_scene_->get_app();
    // without the editor gameplay runs from the first frame; with it, once Play is pressed (first late_update)
    gameplay_started_ = !app->is_editor_enabled();
    visible_ = engineEditorIsGameView();
    encounter_status_ = engineStringCreate(
        gameplay_started_ ? "Defeat all enemies. WASD move, RMB attack, SPACE dodge. Walk into walls to climb." : "Paused. Click Play in the editor toolbar to start.");
    assert(encounter_status_);

    std::vector<engine_ui_document_data_binding_t> bindings{};
    add_binding(bindings, "items_go", items_.go);
    add_binding(bindings, "items_name", items_.name);
    add_binding(bindings, "items_pos_x", items_.pos_x);
    add_binding(bindings, "items_pos_y", items_.pos_y);
    add_binding(bindings, "items_show", items_.show);
    {
        engine_ui_document_data_binding_t binding = {};
        binding.name = "equip";
        binding.data_callback.fn_ptr = &equip_weapon_callback;
        binding.data_callback.user_data = my_scene_;
        binding.type = ENGINE_UI_DATA_TYPE_EVENT_CALLBACK;
        bindings.push_back(binding);
    }
    add_binding(bindings, "enemies_go", enemies_.go);
    add_binding(bindings, "enemies_health", enemies_.progressbar_value);
    add_binding(bindings, "enemies_pos_x", enemies_.pos_x);
    add_binding(bindings, "enemies_pos_y", enemies_.pos_y);
    {
        engine_ui_document_data_binding_t binding{};
        binding.name = "player_health";
        binding.data_uint32_t = &player_health_;
        binding.type = ENGINE_UI_DATA_TYPE_UINT32;
        bindings.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding{};
        binding.name = "player_max_health";
        binding.data_uint32_t = &player_max_health_;
        binding.type = ENGINE_UI_DATA_TYPE_UINT32;
        bindings.push_back(binding);
    }
    {
        engine_ui_document_data_binding_t binding{};
        binding.name = "encounter_status";
        binding.data_string = encounter_status_;
        binding.type = ENGINE_UI_DATA_TYPE_STRING;
        bindings.push_back(binding);
    }

    engineUiDocumentCreateDataHandle("DataModel_Main_UI", bindings.data(), bindings.size(), &handle_);

    engineUiDocumentCreateFromFile("project_c_gameplay_ui.rml", &doc_);
    if (doc_)
    {
        visible_ ? engineUiDocumentShow(doc_) : engineUiDocumentHide(doc_);
        if (app->is_editor_enabled())
        {
            engine_ui_element_t player_hud{};
            const auto player_hud_found = engineUiDocumentGetElementById(doc_, "player_hud", &player_hud);
            const auto player_hud_left_set = player_hud_found == ENGINE_RESULT_CODE_OK
                && engineUiElementSetProperty(player_hud, "left", "22%") == ENGINE_RESULT_CODE_OK;
            const auto player_hud_top_set = player_hud_found == ENGINE_RESULT_CODE_OK
                && engineUiElementSetProperty(player_hud, "top", "65%") == ENGINE_RESULT_CODE_OK;

            engine_ui_element_t encounter_status{};
            const auto encounter_status_found = engineUiDocumentGetElementById(doc_, "encounter_status", &encounter_status);
            const auto encounter_status_top_set = encounter_status_found == ENGINE_RESULT_CODE_OK
                && engineUiElementSetProperty(encounter_status, "top", "18%") == ENGINE_RESULT_CODE_OK;

            if (!player_hud_left_set || !player_hud_top_set || !encounter_status_top_set)
            {
                engineLog("Failed to position gameplay UI inside the editor scene viewport.\n");
            }
        }
    }
}

project_c::GameplayHud::~GameplayHud()
{
    engineUiDataHandleDestroy(handle_);
    engineUiDocumentDestroy(doc_);
    engineStringDestroy(encounter_status_);
}

void project_c::GameplayHud::late_update(float dt)
{
    engine::ScopedProfiler prof("project_c::GameplayHud::late_update");
    // late_update only runs while the game is playing
    if (!gameplay_started_)
    {
        gameplay_started_ = true;
        set_status("WASD move, RMB attack, SPACE dodge. Walk into a wall / window / roof edge to climb, SPACE lets go.");
    }
    update_player_health();
    update_enemies();
    update_items_on_ground();
    update_encounter_status();
}

void project_c::GameplayHud::set_visible(bool visible)
{
    if (visible_ == visible || !doc_)
    {
        return;
    }
    visible_ = visible;
    visible ? engineUiDocumentShow(doc_) : engineUiDocumentHide(doc_);
}

void project_c::GameplayHud::update_player_health()
{
    const auto* player = my_scene_->find_script<Player>();
    if (!player || (player->get_hp() == player_health_ && player->get_max_hp() == player_max_health_))
    {
        return;
    }
    player_health_ = player->get_hp();
    player_max_health_ = player->get_max_hp();
    engineUiDataHandleDirtyVariable(handle_, "player_health");
    engineUiDataHandleDirtyVariable(handle_, "player_max_health");
}

void project_c::GameplayHud::update_enemies()
{
    std::unordered_set<engine_game_object_t> alive;
    for (const auto* enemy : my_scene_->get_scripts<Enemy>())
    {
        alive.insert(enemy->get_game_object());
        update_enemy(enemy);
    }
    // health bars of enemies whose script is gone
    for (auto i = engineVectorSizeUint32(enemies_.go); i > 0; i--)
    {
        if (!alive.contains(engineVectorGetUint32(enemies_.go, i - 1)))
        {
            remove_enemy(static_cast<std::uint32_t>(i - 1));
        }
    }
}

void project_c::GameplayHud::update_items_on_ground()
{
    std::unordered_set<engine_game_object_t> on_ground;
    for (const auto* weapon : my_scene_->get_scripts<Weapon>())
    {
        if (weapon->is_on_ground())
        {
            on_ground.insert(weapon->get_game_object());
            update_item_on_ground(weapon);
        }
    }
    // picked up, or the weapon is gone
    for (auto i = engineVectorSizeUint32(items_.go); i > 0; i--)
    {
        if (!on_ground.contains(engineVectorGetUint32(items_.go, i - 1)))
        {
            remove_item_from_ground(static_cast<std::uint32_t>(i - 1));
        }
    }
}

void project_c::GameplayHud::update_encounter_status()
{
    const auto* encounter = my_scene_->find_script<Encounter>();
    if (!encounter || encounter->get_state() == shown_encounter_state_)
    {
        return;
    }
    shown_encounter_state_ = encounter->get_state();
    switch (encounter->get_state())
    {
    case Encounter::State::eCleared:
        set_status("Encounter clear! Press R to restart.");
        break;
    case Encounter::State::eDefeated:
        set_status("You were defeated. Press R to restart.");
        break;
    case Encounter::State::eInProgress:
        break;
    }
}

void project_c::GameplayHud::set_status(const char* text)
{
    engineStringSet(encounter_status_, text);
    engineUiDataHandleDirtyVariable(handle_, "encounter_status");
}

void project_c::GameplayHud::update_enemy(const Enemy* en)
{
    engine::ScopedProfiler prof("project_c::GameplayHud::update_enemy");
    const auto active_camera_go = utils::get_active_camera_game_objects()[0];
    const auto enemy_go = en->get_game_object();
    auto enemy_tc = engineGameObjectGetTransformComponent(enemy_go);
    auto healthbar_position = enemy_tc.position;
    const auto height_offset = 1.0f; // healthbar need to be on top of the enemy
    healthbar_position[1] += height_offset;
    const auto enemy_screen_coords = engineCameraComponentConvertWorldPositionToScreenPosition(active_camera_go, healthbar_position);

    const auto box_width = 10; // percent, ToDo: get propery from UiElement
    const auto box_height = 1; // percent, ToDo: get propery from UiElement

    // calculate x and y and we need to center the box, so subtract half of the size of the box
    const auto x_str = std::to_string(enemy_screen_coords.x * 100 - (box_width /2)) + "%";
    const auto y_str = std::to_string(enemy_screen_coords.y * 100 - (box_height /2)) + "%";

    // calculate health progress bar
    const auto health_progress_value = static_cast<std::uint32_t>(((float)en->hp / (float)en->max_hp) * 100);

    bool found = false;
    for (auto i = 0; i < engineVectorSizeUint32(enemies_.go); i++)
    {
        const auto go = engineVectorGetUint32(enemies_.go, i);
        if (go == enemy_go)
        {
            found = true;

            engineVectorSetUint32(enemies_.progressbar_value, i, health_progress_value);

            auto pos_x_str = engineVectorGetEngineString(enemies_.pos_x, i);
            engineStringSet(pos_x_str, x_str.c_str());

            auto pos_y_str = engineVectorGetEngineString(enemies_.pos_y, i);
            engineStringSet(pos_y_str, y_str.c_str());
            break;
        }
    }
    if (!found)
    {
        engineVectorPushBackUint32(enemies_.go, enemy_go);
        engineVectorPushBackUint32(enemies_.progressbar_value, health_progress_value);
        engineUiDataHandleDirtyVariable(handle_, "enemies_go");

        engineVectorPushBackEngineString(enemies_.pos_x, engineStringCreate(x_str.c_str()));
        engineVectorPushBackEngineString(enemies_.pos_y, engineStringCreate(y_str.c_str()));
    }

    engineUiDataHandleDirtyVariable(handle_, "enemies_health");
    engineUiDataHandleDirtyVariable(handle_, "enemies_pos_x");
    engineUiDataHandleDirtyVariable(handle_, "enemies_pos_y");
}

void project_c::GameplayHud::remove_enemy(std::uint32_t index)
{
    engineVectorEraseUint32(enemies_.go, index);
    engineVectorEraseUint32(enemies_.progressbar_value, index);
    engineVectorEraseEngineString(enemies_.pos_x, index);
    engineVectorEraseEngineString(enemies_.pos_y, index);

    engineUiDataHandleDirtyVariable(handle_, "enemies_go");
    engineUiDataHandleDirtyVariable(handle_, "enemies_health");
    engineUiDataHandleDirtyVariable(handle_, "enemies_pos_x");
    engineUiDataHandleDirtyVariable(handle_, "enemies_pos_y");
}

void project_c::GameplayHud::update_item_on_ground(const Weapon* weapon)
{
    engine::ScopedProfiler prof("project_c::GameplayHud::update_item_on_ground");
    const auto active_camera_go = utils::get_active_camera_game_objects()[0];
    const auto item_go = weapon->get_game_object();
    const auto item_tc = engineGameObjectGetTransformComponent(item_go);
    const auto item_screen_coords = engineCameraComponentConvertWorldPositionToScreenPosition(active_camera_go, item_tc.position);

    const auto x_str = std::to_string(item_screen_coords.x * 100) + "%";
    const auto y_str = std::to_string(item_screen_coords.y * 100) + "%";

    bool found = false;
    for (auto i = 0; i < engineVectorSizeUint32(items_.go); i++)
    {
        const auto go = engineVectorGetUint32(items_.go, i);
        if (go == item_go)
        {
            found = true;
            engineVectorSetUint32(items_.show, i, 1);

            auto pos_x_str = engineVectorGetEngineString(items_.pos_x, i);
            engineStringSet(pos_x_str, x_str.c_str());

            auto pos_y_str = engineVectorGetEngineString(items_.pos_y, i);
            engineStringSet(pos_y_str, y_str.c_str());
            break;
        }
    }
    if (!found)
    {
        engineVectorPushBackUint32(items_.go, item_go);
        engineUiDataHandleDirtyVariable(handle_, "items_go");
        engineVectorPushBackEngineString(items_.name, engineStringCreate("item"));
        engineUiDataHandleDirtyVariable(handle_, "items_name");

        engineVectorPushBackUint32(items_.show, 1);
        engineVectorPushBackEngineString(items_.pos_x, engineStringCreate(x_str.c_str()));
        engineVectorPushBackEngineString(items_.pos_y, engineStringCreate(y_str.c_str()));
    }

    engineUiDataHandleDirtyVariable(handle_, "items_show");
    engineUiDataHandleDirtyVariable(handle_, "items_pos_x");
    engineUiDataHandleDirtyVariable(handle_, "items_pos_y");
}

void project_c::GameplayHud::remove_item_from_ground(std::uint32_t index)
{
    engineVectorEraseUint32(items_.go, index);
    engineVectorEraseUint32(items_.show, index);
    engineVectorEraseEngineString(items_.name, index);
    engineVectorEraseEngineString(items_.pos_x, index);
    engineVectorEraseEngineString(items_.pos_y, index);

    engineUiDataHandleDirtyVariable(handle_, "items_go");
    engineUiDataHandleDirtyVariable(handle_, "items_show");
    engineUiDataHandleDirtyVariable(handle_, "items_name");
    engineUiDataHandleDirtyVariable(handle_, "items_pos_x");
    engineUiDataHandleDirtyVariable(handle_, "items_pos_y");
}
