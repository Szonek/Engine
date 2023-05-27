#include "ui_scripts.h"

#include "../scenes/pve_scene.h"
#include "../scenes/pvp_scene.h"
#include "scene_manager.h"
#include "iscene.h"
#include "event_types_defs.h"
#include "utils.h"
#include "global_constants.h"
#include "player_paddle_script.h"

pong::PlayerTouchAreaScript::PlayerTouchAreaScript(engine::IScene *my_scene, float start_pos_x, float end_pos_x, const char* name)
    : IScript(my_scene)
{
    auto scene = my_scene_->get_handle();

    auto tc = engineSceneAddRectTransformComponent(scene, go_);
    tc.position_min[0] = start_pos_x;
    tc.position_min[1] = 0.0f;

    tc.position_max[0] = end_pos_x;
    tc.position_max[1] = 1.0f;
    engineSceneUpdateRectTransformComponent(scene, go_, &tc);

    auto ic = engineSceneAddImageComponent(scene, go_);
    set_c_array(ic.color, std::array<float, 4>{0.2f, 0.6f, 0.2f, 0.0f});
    engineSceneUpdateImageComponent(scene, go_, &ic);

    auto name_comp = engineSceneAddNameComponent(scene, go_);
    std::memcpy(name_comp.name, name, std::strlen(name));
    engineSceneUpdateNameComponent(scene, go_, &name_comp);
}

void pong::PlayerTouchAreaScript::on_pointer_down(const engine::PointerEventData* ped)
{
    assert(player_script_ != nullptr);
    if (ped->button == ENGINE_MOUSE_BUTTON_LEFT)
    {
        const float full_screen_height = K_CAMERA_ORTHO_SCALE * 2.0f;
        const float top_wall_screenspace_pos = (K_CAMERA_ORTHO_SCALE + K_WALL_Y_OFFSET) / full_screen_height;
        const float bottom_wall_screenspace_pos = (K_CAMERA_ORTHO_SCALE - K_WALL_Y_OFFSET) / full_screen_height;
        const float screenspace_table_height = top_wall_screenspace_pos - bottom_wall_screenspace_pos;
        const float pointer_pos = ped->position[1];
        const float pointer_screenspace_scaled_to_table = ( pointer_pos * screenspace_table_height) + bottom_wall_screenspace_pos;
        // map to range <-1, 1> and scale to world space size  (world middle is 0,0, so we need negative values to go down/left)!
        const float pointer_world_space = K_CAMERA_ORTHO_SCALE * ((pointer_screenspace_scaled_to_table * 2.0f) - 1.0f);
        player_script_->set_target_worldspace_position(pointer_world_space);
    }
}

pong::LeftPlayerTouchAreaScript::LeftPlayerTouchAreaScript(engine::IScene *my_scene)
    : PlayerTouchAreaScript(my_scene, 0.0f, 0.2f, "left_touch")
{
}


pong::RightPlayerTouchAreaScript::RightPlayerTouchAreaScript(engine::IScene *my_scene)
    : PlayerTouchAreaScript(my_scene, 0.8f, 1.0f, "right_touch")
{
}

pong::MainMenuStartPveScene::MainMenuStartPveScene(engine::IScene *my_scene)
    : IScript(my_scene)
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    auto tc = engineSceneAddRectTransformComponent(scene, go_);
    tc.position_min[0] = 0.4f;
    tc.position_min[1] = 0.5f;

    tc.position_max[0] = 0.6f;
    tc.position_max[1] = 0.7f;
    engineSceneUpdateRectTransformComponent(scene, go_, &tc);

    auto ic = engineSceneAddImageComponent(scene, go_);
    set_c_array(ic.color, std::array<float, 4>{0.0f, 0.8f, 0.3f, 0.0f});
    engineSceneUpdateImageComponent(scene, go_, &ic);

    const char* name = "main_menu_start_pve";
    auto name_comp = engineSceneAddNameComponent(scene, go_);
    std::memcpy(name_comp.name, name, std::strlen(name));
    engineSceneUpdateNameComponent(scene, go_, &name_comp);

    auto text_go = go_;//engineSceneCreateGameObject(scene);
    auto text_component = engineSceneAddTextComponent(scene, text_go);
    text_component.font_handle = engineApplicationGetFontByName(app, "tahoma_font");
    assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
    text_component.text = "PVE";
    set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 0.0f});
    engineSceneUpdateTextComponent(scene, text_go, &text_component);

}

void pong::MainMenuStartPveScene::on_pointer_click(const engine::PointerEventData *ped)
{
    assert(my_scene_);
    my_scene_->deactivate();
    get_scene_manager()->get_scene(pong::PveScene::get_name())->activate();
}

pong::MainMenuStartPvpScene::MainMenuStartPvpScene(engine::IScene *my_scene)
    : IScript(my_scene)
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    auto tc = engineSceneAddRectTransformComponent(scene, go_);
    tc.position_min[0] = 0.4f;
    tc.position_min[1] = 0.1f;

    tc.position_max[0] = 0.6f;
    tc.position_max[1] = 0.3f;
    engineSceneUpdateRectTransformComponent(scene, go_, &tc);

    auto ic = engineSceneAddImageComponent(scene, go_);
    set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
    engineSceneUpdateImageComponent(scene, go_, &ic);

    const char* name = "main_menu_start_pvp";
    auto name_comp = engineSceneAddNameComponent(scene, go_);
    std::memcpy(name_comp.name, name, std::strlen(name));
    engineSceneUpdateNameComponent(scene, go_, &name_comp);

    auto text_go = go_;//engineSceneCreateGameObject(scene);
    auto text_component = engineSceneAddTextComponent(scene, text_go);
    text_component.font_handle = engineApplicationGetFontByName(app, "tahoma_font");
    assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
    text_component.text = "PVP";
    set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 0.0f});
    engineSceneUpdateTextComponent(scene, text_go, &text_component);
}

void pong::MainMenuStartPvpScene::on_pointer_click(const engine::PointerEventData *ped)
{
    my_scene_->deactivate();
    get_scene_manager()->get_scene(pong::PvpScene::get_name())->activate();
}

void pong::PlayerSuperPower_TouchAreaScript::on_pointer_click(const engine::PointerEventData *ped)
{
    assert(player_script_ != nullptr);
    if(player_script_->get_super_power_type() != SuperPowerType::eNone)
    {
        player_script_->trigger_super_power();
    }
}

void pong::PlayerSuperPower_TouchAreaScript::update(float dt)
{
    assert(player_script_ != nullptr);
    auto scene = my_scene_->get_handle();
    auto ic = engineSceneGetImageComponent(scene, go_);
    if(player_script_->get_super_power_type() != SuperPowerType::eNone)
    {
        set_c_array(ic.color, std::array<float, 4>{0.05f, 0.9f, 0.05f, 0.0f});
        engineSceneUpdateImageComponent(scene, go_, &ic);
    }
    else
    {
        set_c_array(ic.color, std::array<float, 4>{0.9f, 0.05f, 0.05f, 0.0f});
        engineSceneUpdateImageComponent(scene, go_, &ic);
    }
}

pong::PlayerSuperPower_TouchAreaScript::PlayerSuperPower_TouchAreaScript(engine::IScene* my_scene, float start_pos_x, float end_pos_x, const char* name)
    : IScript(my_scene)
{
    auto scene = my_scene_->get_handle();

    auto tc = engineSceneAddRectTransformComponent(scene, go_);
    tc.position_min[0] = start_pos_x;
    tc.position_min[1] = 0.0f;

    tc.position_max[0] = end_pos_x;
    tc.position_max[1] = 0.1f;
    engineSceneUpdateRectTransformComponent(scene, go_, &tc);

    auto ic = engineSceneAddImageComponent(scene, go_);
    set_c_array(ic.color, std::array<float, 4>{0.9f, 0.05f, 0.05f, 0.0f});
    engineSceneUpdateImageComponent(scene, go_, &ic);

    auto name_comp = engineSceneAddNameComponent(scene, go_);
    std::memcpy(name_comp.name, name, std::strlen(name));
    engineSceneUpdateNameComponent(scene, go_, &name_comp);
}

pong::LeftPlayerSuperPower_0_TouchAreaScript::LeftPlayerSuperPower_0_TouchAreaScript(engine::IScene* my_scene)
    : PlayerSuperPower_TouchAreaScript(my_scene, 0.2f, 0.3f, "left_superpower_toucharea")
{
}

pong::RightPlayerSuperPower_0_TouchAreaScript::RightPlayerSuperPower_0_TouchAreaScript(engine::IScene* my_scene)
    : PlayerSuperPower_TouchAreaScript(my_scene, 0.7f, 0.8f, "right_superpower_toucharea")
{
}



