#include "ball_script.h"
#include "global_constants.h"

#include "utils.h"

#include <cassert>

pong::BallScript::BallScript(engine_application_t& app, engine_scene_t& scene) 
    : IScript(app, scene)
{
    auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
    mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "sphere");
    assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for ball script!");
    engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.scale[0] = 0.5f;
    tc.scale[1] = 0.5f;
    tc.scale[2] = 0.5f;
    engineSceneUpdateTransformComponent(scene_, go_, &tc);

    auto rb = engineSceneAddRigidBodyComponent(scene, go_);
    rb.mass = 1.0f;
    engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
    auto bc = engineSceneAddColliderComponent(scene, go_);
    bc.type = ENGINE_COLLIDER_TYPE_SPHERE;
    bc.collider.sphere.radius = 1.0f;
    bc.friction_static = 0.0f;
    bc.bounciness = 1.0f;
    engineSceneUpdateColliderComponent(scene, go_, &bc);

    auto material_comp = engineSceneAddMaterialComponent(scene, go_);
    set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });
    engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    auto nc = engineSceneAddNameComponent(scene, go_);
    const std::string name = fmt::format("ball");
    std::strcpy(nc.name, name.c_str());
    engineSceneUpdateNameComponent(scene, go_, &nc);

    reset_state();
}

void pong::BallScript::reset_state()
{
    ball_speed_x_ = 20.0f;
    ball_speed_y_ = 20.0f;

    auto tc = engineSceneGetTransformComponent(scene_, go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    engineSceneUpdateTransformComponent(scene_, go_, &tc);

    update_linear_velocity(1.0f, 0.0f);
}

void pong::BallScript::update_linear_velocity(float dir_x, float dir_y)
{
    auto rb = engineSceneGetRigidBodyComponent(scene_, go_);
    rb.linear_velocity[0] = dir_x * ball_speed_x_;
    rb.linear_velocity[1] = dir_y * ball_speed_y_;
    engineSceneUpdateRigidBodyComponent(scene_, go_, &rb);
}

void pong::BallScript::update_linear_velocity(std::span<const float> dir)
{
    assert(dir.size() == 2);
    update_linear_velocity(dir[0], dir[1]);
}

std::array<float, 2> pong::BallScript::get_direction_vector() const
{
    auto rb = engineSceneGetRigidBodyComponent(scene_, go_);
    return {
        rb.linear_velocity[0] / ball_speed_x_,
        rb.linear_velocity[1] / ball_speed_y_,
    };
}

void pong::BallScript::update(float dt)
{
    handle_input(dt);

    timer_accu += dt;

    // update speed every 2 seconds
    if (timer_accu >= 2000.0f)
    {
        ball_speed_x_ += 2.0f;
        ball_speed_y_ += 2.0f;
        timer_accu = 0.0f;
        update_linear_velocity(get_direction_vector());
    }
}

void pong::BallScript::handle_input(float dt)
{
    auto tc = engineSceneGetTransformComponent(scene_, go_);

    engine_fingers_infos_list_t finger_infos{};
    const auto has_finger_info = engineApplicationGetFingerInfo(app_, &finger_infos);

    bool should_reset_state = false;

    if constexpr (K_IS_ANDROID)
    {
        if (has_finger_info)
        {
            for (std::size_t i = 0; i < ENGINE_FINGERS_INFOS_LIST_COUNT; i++)
            {
                const auto f = finger_infos.infos[i];
                if (f.x > 0.35f && f.x < 0.65 && f.y < 0.2f)
                {
                    should_reset_state = true;
                }
            }
        }
    }
    else
    {
        if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_SPACE))
        {
            should_reset_state = true;
        }
    }

    if (should_reset_state)
    {
        reset_state();
    }
}