#include "ui_scripts.h"

#include "iscene.h"
#include "event_types_defs.h"
#include "utils.h"
#include "global_constants.h"
#include "player_paddle_script.h"

pong::PlayerTouchAreaScript::PlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene, float start_pos_x, float end_pos_x, const char* name)
    : IScript(app, scene)
{
    auto tc = engineSceneAddRectTransformComponent(scene, go_);
    tc.position_min[0] = start_pos_x;
    tc.position_min[1] = 0.0f;

    tc.position_max[0] = end_pos_x;
    tc.position_max[1] = 1.0f;
    engineSceneUpdateRectTransformComponent(scene, go_, &tc);

    auto ic = engineSceneAddImageComponent(scene, go_);
    set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
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
        player_script_->set_target_screenspace_position(ped->position[1]);
    }
}

pong::LeftPlayerTouchAreaScript::LeftPlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene)
    : PlayerTouchAreaScript(app, scene, 0.0f, 0.2f, "left_touch")
{
}


pong::RightPlayerTouchAreaScript::RightPlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene)
    : PlayerTouchAreaScript(app, scene, 0.8f, 1.0f, "right_touch")
{
}

pong::MainMenuStartPveScene::MainMenuStartPveScene(engine_application_t &app, engine_scene_t &scene)
    : IScript(app, scene)
{
    auto tc = engineSceneAddRectTransformComponent(scene, go_);
    tc.position_min[0] = 0.4f;
    tc.position_min[1] = 0.5f;

    tc.position_max[0] = 0.6f;
    tc.position_max[1] = 0.7f;
    engineSceneUpdateRectTransformComponent(scene, go_, &tc);

    auto ic = engineSceneAddImageComponent(scene, go_);
    set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
    engineSceneUpdateImageComponent(scene, go_, &ic);

    const char* name = "main_menu_start_pve";
    auto name_comp = engineSceneAddNameComponent(scene, go_);
    std::memcpy(name_comp.name, name, std::strlen(name));
    engineSceneUpdateNameComponent(scene, go_, &name_comp);
}

void pong::MainMenuStartPveScene::on_pointer_click(const engine::PointerEventData *ped)
{
    assert(my_scene_);
    assert(pve_scene_);
    my_scene_->deactivate();
    pve_scene_->activate();
}
