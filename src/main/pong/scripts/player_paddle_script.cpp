#include "player_paddle_script.h"
#include "ball_script.h"

#include "utils.h"
#include "global_constants.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>

pong::PlayerPaddleScript::PlayerPaddleScript(engine_application_t& app, engine_scene_t& scene, float init_pos_x, float score_init_pos_x, const char* name)
    : IScript(app, scene)
    , score_(0)
    , score_str_(std::to_string(score_))
{
    auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
    mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "cube");
    assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player paddle script!");
    engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = init_pos_x;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.5f;
    tc.scale[1] = 2.5f;
    tc.scale[2] = 1.0f;
    engineSceneUpdateTransformComponent(scene_, go_, &tc);

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
        auto text_component = engineSceneAddTextComponent(scene, score_go_);
        text_component.font_handle = engineApplicationGetFontByName(app_, "tahoma_font");
        assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
        text_component.text = score_str_.c_str();
        set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        engineSceneUpdateTextComponent(scene, score_go_, &text_component);

        auto tc = engineSceneAddRectTransformComponent(scene, score_go_);
        tc.position[0] = score_init_pos_x;
        tc.position[1] = 0.85f;

        tc.scale[0] = 1.5f;
        tc.scale[1] = 1.5f;
        engineSceneUpdateRectTransformComponent(scene, score_go_, &tc);
    }
}

void pong::PlayerPaddleScript::on_collision(const collision_t& info)
{
    assert(ball_script_);

    if (info.other == ball_script_->get_game_object())
    {
        const auto paddle_tc = engineSceneGetTransformComponent(scene_, go_);
        const auto paddle_current_y = paddle_tc.position[1];

        auto ball_tc = engineSceneGetTransformComponent(scene_, info.other);
        const auto ball_current_y = ball_tc.position[1];

        const auto interct_pos = -1.0f * ((paddle_current_y - ball_current_y)) / 2.5f;
        //std::cout << interct_pos << std::endl;
        engineLog(fmt::format("{} \n", interct_pos).c_str());
        auto ball_dir = ball_script_->get_direction_vector();
        ball_dir[1] = interct_pos;
        const auto ball_dir_normalized = glm::normalize(glm::make_vec2(ball_dir.data()));
        ball_script_->update_linear_velocity(ball_dir_normalized[0], ball_dir_normalized[1]);
        //auto rb = engineSceneGetRigidBodyComponent(scene_, info.other);
        //rb->linear_velocity[0] = 0.0f;
        //rb.linear_velocity[1] = 10.0f * interct_pos;
        //engineSceneUpdateRigidBodyComponent(scene_, info.other, &rb);
    }
}

void pong::PlayerPaddleScript::update(float dt)
{
    handle_input(dt);
    score_str_ = std::to_string(score_);
}

void pong::PlayerPaddleScript::set_score(std::size_t new_score)
{
    score_ = new_score;
}

std::size_t pong::PlayerPaddleScript::get_score() const
{
    return score_;
}

void pong::PlayerPaddleScript::handle_input(float dt)
{
    auto tc = engineSceneGetTransformComponent(scene_, go_);
    const engine_finger_info_t* finger_infos = nullptr;
    std::size_t fingers_info_count = 0;
    const auto has_finger_info = engineApplicationGetFingerInfo(app_, &finger_infos, &fingers_info_count);

    bool update_component = false;
    if constexpr (K_IS_ANDROID)
    {
        if (has_finger_info)
        {
            for (std::size_t i = 0; i < fingers_info_count; i++)
            {
                const auto f = finger_infos[i];
                if ((f.event_type_flags & ENGINE_FINGER_DOWN || f.event_type_flags & ENGINE_FINGER_MOTION)
                    && is_finger_in_controller_area_impl(f))
                {
                    const auto y_delta = -1.0f * ((f.y * 2.0f) - 1.0f);
                    tc.position[1] = y_delta * 8.0f;
                    update_component = true;
                }
            }
        }
    }
    else
    {
        // KEYBOARD
        if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_W))
        {
            tc.position[1] += 0.05f * dt;
            update_component = true;
        }
        if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_S))
        {
            tc.position[1] -= 0.05f * dt;
            update_component = true;
        }
    }
    if (update_component)
    {
        engineSceneUpdateTransformComponent(scene_, go_, &tc);
    }
}

// 
// --- RIGHT PLAYER ---
//

pong::RightPlayerPaddleScript::RightPlayerPaddleScript(engine_application_t& app, engine_scene_t& scene)
    : PlayerPaddleScript(app, scene, 12.0f, 0.75f, "right_player")
{
    // text component
    {
        const auto text_go = engineSceneCreateGameObject(scene);
        auto text_component = engineSceneAddTextComponent(scene, text_go);
        text_component.font_handle = engineApplicationGetFontByName(app_, "tahoma_font");
        assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
        text_component.text = "Player 2";
        set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        engineSceneUpdateTextComponent(scene, text_go, &text_component);

        auto tc = engineSceneAddRectTransformComponent(scene, text_go);
        tc.position[0] = 0.75f;
        tc.position[1] = 0.15f;

        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.5f;
        engineSceneUpdateRectTransformComponent(scene, text_go, &tc);
    }

    // touchable area component
    {
        const auto touch_area_go = engineSceneCreateGameObject(scene);
        auto tc = engineSceneAddRectTransformComponent(scene, touch_area_go);
        tc.position[0] = 0.8f;
        tc.position[1] = 0.0f;

        tc.scale[0] = 1.0f;
        tc.scale[1] = 1.0f;
        engineSceneUpdateRectTransformComponent(scene, touch_area_go, &tc);

        auto ic = engineSceneAddImageComponent(scene, touch_area_go);
        set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
        engineSceneUpdateImageComponent(scene, touch_area_go, &ic);
    }
}

bool pong::RightPlayerPaddleScript::is_finger_in_controller_area_impl(const engine_finger_info_t& f)
{
    return (f.x > 0.8f && f.x <= 1.0f);
}

// 
// --- LEFT PLAYER ---
//
pong::LeftPlayerPaddleScript::LeftPlayerPaddleScript(engine_application_t& app, engine_scene_t& scene)
    : PlayerPaddleScript(app, scene, -12.0f, 0.25f, "left_player")
{
    // text component for the NAME
    {
        const auto text_go = engineSceneCreateGameObject(scene);
        auto text_component = engineSceneAddTextComponent(scene, text_go);
        text_component.font_handle = engineApplicationGetFontByName(app_, "tahoma_font");
        assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
        text_component.text = "Player 1";
        set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        engineSceneUpdateTextComponent(scene, text_go, &text_component);

        auto tc = engineSceneAddRectTransformComponent(scene, text_go);
        tc.position[0] = 0.25f;
        tc.position[1] = 0.15f;

        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.5f;
        engineSceneUpdateRectTransformComponent(scene, text_go, &tc);
    }

    // touchable area component
    {
        const auto touch_area_go = engineSceneCreateGameObject(scene);
        auto tc = engineSceneAddRectTransformComponent(scene, touch_area_go);
        tc.position[0] = 0.0f;
        tc.position[1] = 0.0f;

        tc.scale[0] = 0.2f;
        tc.scale[1] = 1.0f;
        engineSceneUpdateRectTransformComponent(scene, touch_area_go, &tc);

        auto ic = engineSceneAddImageComponent(scene, touch_area_go);
        set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
        engineSceneUpdateImageComponent(scene, touch_area_go, &ic);
    }
}

bool pong::LeftPlayerPaddleScript::is_finger_in_controller_area_impl(const engine_finger_info_t& f)
{
    return (f.x < 0.2f && f.x >= 0.0f);
}