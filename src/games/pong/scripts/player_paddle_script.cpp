#include "player_paddle_script.h"
#include "ball_script.h"

#include "iscene.h"
#include "utils.h"
#include "global_constants.h"
#include "event_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>

pong::PlayerPaddleScript::PlayerPaddleScript(engine::IScene *my_scene, float init_pos_x, float score_init_pos_x, const char* name)
    : IScript(my_scene)
    , score_(0)
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
    mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
    assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player paddle script!");
    engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = init_pos_x;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.5f;
    tc.scale[1] = 2.5f;
    tc.scale[2] = 1.0f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    auto bc = engineSceneAddColliderComponent(scene, go_);
    bc.type = ENGINE_COLLIDER_TYPE_BOX;
    bc.bounciness = 1.0f;
    bc.friction_static = 0.0f;
    engineSceneUpdateColliderComponent(scene, go_, &bc);

    auto material_comp = engineSceneAddMaterialComponent(scene, go_);
    set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });
    engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    auto nc = engineSceneAddNameComponent(scene, go_);
    std::strcpy(nc.name, name);
    engineSceneUpdateNameComponent(scene, go_, &nc);

    // text component for the SCORE
    {
        score_go_ = engineSceneCreateGameObject(scene);

        auto tc = engineSceneAddRectTransformComponent(scene, score_go_);
        tc.position_min[0] = score_init_pos_x;
        tc.position_min[1] = 0.85f;

        tc.position_max[0] = 1.0f;
        tc.position_max[1] = 1.0f;
        engineSceneUpdateRectTransformComponent(scene, score_go_, &tc);
    }

    my_scene_->register_event_callback(PONG_EVENT_TYPE_GOAL_SCORED, [this]()
        {
            const auto super_speed_factor = 2.0f;
            const auto ball_speed = ball_script_->get_speed();
            ball_script_->update_diffuse_color(std::array<float, 4>{0.4f, 0.3f, 1.0f, 0.0f});
        }
    );
}

void pong::PlayerPaddleScript::on_collision(const collision_t& info)
{
    assert(ball_script_);

    if (info.other == ball_script_->get_game_object())
    {
        const auto paddle_tc = engineSceneGetTransformComponent(my_scene_->get_handle(), go_);
        const auto paddle_current_y = paddle_tc.position[1];

        auto ball_tc = engineSceneGetTransformComponent(my_scene_->get_handle(), info.other);
        const auto ball_current_y = ball_tc.position[1];

        const auto interct_pos = -1.0f * ((paddle_current_y - ball_current_y)) / 2.5f;
        //std::cout << interct_pos << std::endl;
        //engineLog(fmt::format("{} \n", interct_pos).c_str());
        auto ball_dir = ball_script_->get_direction_vector();
        ball_dir[1] = interct_pos;
        const auto ball_dir_normalized = glm::normalize(glm::make_vec2(ball_dir.data()));
        ball_script_->update_linear_velocity(ball_dir_normalized[0], ball_dir_normalized[1]);
    }
}

void pong::PlayerPaddleScript::update(float dt)
{
    auto tc = engineSceneGetTransformComponent(my_scene_->get_handle(), go_);
    if(tc.position[1] != target_y)
    {
       tc.position[1] = target_y;
       engineSceneUpdateTransformComponent(my_scene_->get_handle(), go_, &tc);
    }

}

void pong::PlayerPaddleScript::set_score(std::size_t new_score)
{

    score_ = new_score;
}

std::size_t pong::PlayerPaddleScript::get_score() const
{
    return score_;
}

void pong::PlayerPaddleScript::set_target_worldspace_position(float y)
{
    target_y = y;
}

void pong::PlayerPaddleScript::trigger_super_power()
{
    //super_power_state_ = SuperPowerState::eTrigger;
}

// 
// --- RIGHT PLAYER ---
//

pong::RightPlayerPaddleScript::RightPlayerPaddleScript(engine::IScene *my_scene)
    : PlayerPaddleScript(my_scene, 12.0f, 0.75f, "right_player")
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    // text component
    {
        const auto text_go = engineSceneCreateGameObject(scene);
        auto text_component = engineSceneAddTextComponent(scene, text_go);
        text_component.font_handle = engineApplicationGetFontByName(app, "tahoma_font");
        assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
        text_component.text = "Player 2";
        text_component.scale[0] = 0.25f;
        text_component.scale[1] = 0.25f;
        set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        engineSceneUpdateTextComponent(scene, text_go, &text_component);

        auto tc = engineSceneAddRectTransformComponent(scene, text_go);
        tc.position_min[0] = 0.55f;
        tc.position_min[1] = 0.15f;

        tc.position_max[0] = 1.0f;
        tc.position_max[1] = 1.0f;
        engineSceneUpdateRectTransformComponent(scene, text_go, &tc);
    }
}
// 
// --- LEFT PLAYER ---
//
pong::LeftPlayerPaddleScript::LeftPlayerPaddleScript(engine::IScene *my_scene)
    : PlayerPaddleScript(my_scene, -12.0f, 0.25f, "left_player")
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    // text component for the NAME
    {
        const auto text_go = engineSceneCreateGameObject(scene);
        auto text_component = engineSceneAddTextComponent(scene, text_go);
        text_component.font_handle = engineApplicationGetFontByName(app, "tahoma_font");
        assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
        text_component.text = "Player 1";
        text_component.scale[0] = 0.25f;
        text_component.scale[1] = 0.25f;
        set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        engineSceneUpdateTextComponent(scene, text_go, &text_component);

        auto tc = engineSceneAddRectTransformComponent(scene, text_go);
        tc.position_min[0] = 0.25f;
        tc.position_min[1] = 0.15f;

        tc.position_max[0] = 1.0f;
        tc.position_max[1] = 1.0f;
        engineSceneUpdateRectTransformComponent(scene, text_go, &tc);
    }
}