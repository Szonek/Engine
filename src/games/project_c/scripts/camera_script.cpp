#include "camera_script.h"
#include "scripts_utils.h"

#include "iscene.h"

#include <algorithm>
#include <cmath>

namespace
{
// Half of the visible world height (world units). Smaller = more zoomed in.
constexpr float ORTHO_HALF_HEIGHT = 5.0f;
// Classic 3/4 top-down view. No yaw, so WASD stays aligned with the screen axes.
constexpr float CAMERA_PITCH_DEGREES = 45.0f;
// Orthographic projection has no perspective, so the distance doesn't change the size of anything on screen.
// It only has to be big enough to keep tall geometry in front of the near plane
// and small enough to keep the visible ground inside of the shadow distance.
constexpr float CAMERA_DISTANCE = 20.0f;
const float CAMERA_OFFSET_Y = CAMERA_DISTANCE * std::sin(CAMERA_PITCH_DEGREES * 3.14159265f / 180.0f);
const float CAMERA_OFFSET_Z = CAMERA_DISTANCE * std::cos(CAMERA_PITCH_DEGREES * 3.14159265f / 180.0f);
} // namespace

project_c::CameraScript::CameraScript(engine::IScene* my_scene)
    : BaseNode(my_scene, "camera")
{
    auto camera_comp = engineGameObjectAddCameraComponent(go_);
    camera_comp.enabled = true;
    camera_comp.clip_plane_near = 0.1f;
    camera_comp.clip_plane_far = 100.0f;
    camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
    camera_comp.type_union.orthographics_scale = ORTHO_HALF_HEIGHT;

    camera_comp.target[0] = 0.0f;
    camera_comp.target[1] = 0.0f;
    camera_comp.target[2] = 0.0f;

    engineGameObjectUpdateCameraComponent(go_, &camera_comp);

    auto camera_transform_comp = engineGameObjectAddTransformComponent(go_);
    camera_transform_comp.position[0] = 0.0f;
    camera_transform_comp.position[1] = CAMERA_OFFSET_Y;
    camera_transform_comp.position[2] = CAMERA_OFFSET_Z;
    engineGameObjectUpdateTransformComponent(go_, &camera_transform_comp);
}

void project_c::CameraScript::late_update(float dt)
{
    engine::ScopedProfiler profiler("project_c::CameraScript::late_update");
    const auto mouse_coords = engineMouseCoordsGet();

    const float move_speed = 1.0f * dt;
    if (!update_follow_target())
    {
        return;
    }

    if (shake_remaining_seconds_ > 0.0f)
    {
        const float dt_seconds = dt / 1000.0f;
        shake_elapsed_seconds_ += dt_seconds;
        shake_remaining_seconds_ = std::max(0.0f, shake_remaining_seconds_ - dt_seconds);
        const float fade = shake_remaining_seconds_ / shake_duration_seconds_;
        const float shake_x = std::sin(shake_elapsed_seconds_ * 83.0f) * shake_intensity_ * fade;
        const float shake_y = std::sin(shake_elapsed_seconds_ * 117.0f) * shake_intensity_ * 0.55f * fade;
        // orthographic camera: moving only the eye is barely visible, the target has to move with it
        auto tc = engineGameObjectGetTransformComponent(go_);
        tc.position[0] += shake_x;
        tc.position[1] += shake_y;
        engineGameObjectUpdateTransformComponent(go_, &tc);
        auto camera_comp = engineGameObjectGetCameraComponent(go_);
        camera_comp.target[0] += shake_x;
        camera_comp.target[1] += shake_y;
        engineGameObjectUpdateCameraComponent(go_, &camera_comp);
    }
}

void project_c::CameraScript::snap_to_player()
{
    update_follow_target();
}

bool project_c::CameraScript::update_follow_target()
{
    const auto all_players = utils::get_game_objects_with_name("player");
    const auto character_go = all_players.size() > 0 ? all_players[0] : ENGINE_INVALID_GAME_OBJECT_ID;
    if (character_go == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return false;
    }

    const auto character_tc = engineGameObjectGetTransformComponent(character_go);
    auto camera_tc = engineGameObjectGetTransformComponent(go_);
    camera_tc.position[0] = character_tc.position[0];
    camera_tc.position[1] = character_tc.position[1] + CAMERA_OFFSET_Y;
    camera_tc.position[2] = character_tc.position[2] + CAMERA_OFFSET_Z;

    auto camera_comp = engineGameObjectGetCameraComponent(go_);
    camera_comp.target[0] = character_tc.position[0];
    camera_comp.target[1] = character_tc.position[1];
    camera_comp.target[2] = character_tc.position[2];
    engineGameObjectUpdateCameraComponent(go_, &camera_comp);
    engineGameObjectUpdateTransformComponent(go_, &camera_tc);
    return true;
}

void project_c::CameraScript::shake(float intensity, float duration_seconds)
{
    if (intensity <= 0.0f || duration_seconds <= 0.0f)
    {
        return;
    }

    if (shake_remaining_seconds_ <= 0.0f)
    {
        shake_elapsed_seconds_ = 0.0f;
    }
    shake_duration_seconds_ = std::max(shake_duration_seconds_, duration_seconds);
    shake_remaining_seconds_ = std::max(shake_remaining_seconds_, duration_seconds);
    shake_intensity_ = std::max(shake_intensity_, intensity);
}