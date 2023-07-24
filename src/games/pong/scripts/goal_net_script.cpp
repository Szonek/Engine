#include "goal_net_script.h"
#include "global_constants.h"
#include "event_types.h"

#include "iscene.h"
#include "ball_script.h"
#include "player_paddle_script.h"

#include "utils.h"

#include <cassert>

pong::GoalNetScript::GoalNetScript(engine::IScene *my_scene, float init_pos_x, const char* name)
    : IScript(my_scene)
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
    mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
    assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player goal net script!");
    mesh_comp.disable = K_IS_GOAL_NET_DISABLE_RENDER;
    engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = init_pos_x;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.5f;
    tc.scale[1] = 25.0f;
    tc.scale[2] = 1.0f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    auto bc = engineSceneAddColliderComponent(scene, go_);
    bc.type = ENGINE_COLLIDER_TYPE_BOX;
    bc.is_trigger = true;
    engineSceneUpdateColliderComponent(scene, go_, &bc);

    auto material_comp = engineSceneAddMaterialComponent(scene, go_);
    set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.2f });
    engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    auto nc = engineSceneAddNameComponent(scene, go_);
    std::strcpy(nc.name, name);
    engineSceneUpdateNameComponent(scene, go_, &nc);

}

void pong::GoalNetScript::update(float dt)
{
    if (score_fence_.was_score)
    {
        score_fence_.frame_counter++;
    }
    if (score_fence_.frame_counter == score_collision_fence::COUNTER_LIMIT)
    {
        score_fence_.frame_counter = 0;
        score_fence_.was_score = false;
    }
}

void pong::GoalNetScript::on_collision(const collision_t& info)
{
    assert(ball_script_ != nullptr);
    assert(player_paddel_script_ != nullptr);

    if (info.other == ball_script_->get_game_object() && score_fence_.frame_counter == 0)
    {
       // ball_script_->reset_state();
        player_paddel_script_->set_score(player_paddel_script_->get_score() + 1);
        score_fence_.was_score = true;
        my_scene_->get_user_event_sysmte()->activate_event(PONG_EVENT_TYPE_GOAL_SCORED);
    }

}

pong::LeftGoalNetScript::LeftGoalNetScript(engine::IScene *my_scene)
    : GoalNetScript(my_scene, -15.0f, "left_goal_net")
{
}

pong::RightGoalNetScript::RightGoalNetScript(engine::IScene *my_scene)
     : GoalNetScript(my_scene, 15.0f, "right_goal_net")
{
}