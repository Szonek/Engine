#include "camera_script.h"
#include "scripts_utils.h"

#include "iscene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>
#include <vector>

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
const glm::vec3 CAMERA_OFFSET(0.0f, CAMERA_OFFSET_Y, CAMERA_OFFSET_Z);

// Follow: critically damped spring. Tight horizontally (combat), softer vertically (climbing, vaults, drops).
constexpr float FOLLOW_SMOOTH_TIME_HORIZONTAL = 0.1f;
constexpr float FOLLOW_SMOOTH_TIME_VERTICAL = 0.3f;
// Look-ahead: camera shifts part of the way toward the cursor, so more of the fight in the aimed direction is visible.
// The cursor's world position depends on the camera itself, effective shift is FRACTION / (1 - FRACTION) of the cursor offset.
constexpr float LOOK_AHEAD_FRACTION = 0.2f;
constexpr float LOOK_AHEAD_MAX_DISTANCE = 2.0f;
constexpr float LOOK_AHEAD_SMOOTH_TIME = 0.25f;
// Long frames (loading, breakpoints) would otherwise make the camera jump.
constexpr float MAX_DELTA_SECONDS = 0.1f;

// Occluders between the camera and the player are dithered out.
constexpr float OCCLUDER_FADE = 0.75f;              // not fully invisible, the wall shape stays readable
constexpr float OCCLUDER_FADE_OUT_SECONDS = 0.15f;  // time to fade out an occluder
constexpr float OCCLUDER_FADE_IN_SECONDS = 0.3f;    // time to bring it back
constexpr std::size_t OCCLUDER_MAX_HITS_PER_RAY = 6;
// rays are cast from a few points of the player's body toward the camera
const std::array<glm::vec3, 5> OCCLUSION_SAMPLE_OFFSETS = {
    glm::vec3( 0.0f, 0.2f, 0.0f),
    glm::vec3( 0.0f, 0.5f, 0.0f),
    glm::vec3( 0.0f, 0.85f, 0.0f),
    glm::vec3(-0.3f, 0.5f, 0.0f),
    glm::vec3( 0.3f, 0.5f, 0.0f),
};

float smooth_damp(float current, float target, float& velocity, float smooth_time, float delta_seconds)
{
    // critically damped spring (same approximation as Unity's SmoothDamp), stable for any delta time
    const float omega = 2.0f / std::max(smooth_time, 0.0001f);
    const float x = omega * delta_seconds;
    const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float change = current - target;
    const float temp = (velocity + omega * change) * delta_seconds;
    velocity = (velocity - omega * temp) * decay;
    return target + (change + temp) * decay;
}

glm::vec3 smooth_damp(const glm::vec3& current, const glm::vec3& target, glm::vec3& velocity, const glm::vec3& smooth_time, float delta_seconds)
{
    return {
        smooth_damp(current.x, target.x, velocity.x, smooth_time.x, delta_seconds),
        smooth_damp(current.y, target.y, velocity.y, smooth_time.y, delta_seconds),
        smooth_damp(current.z, target.z, velocity.z, smooth_time.z, delta_seconds)
    };
}

void set_mesh_fade(engine_game_object_t go, float fade)
{
    if (!engineGameObjectIsValid(go) || !engineGameObjectHasMeshComponent(go))
    {
        return;
    }
    auto mc = engineGameObjectGetMeshComponent(go);
    mc.fade = fade;
    engineGameObjectUpdateMeshComponent(go, &mc);
}
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
    camera_transform_comp.position[0] = CAMERA_OFFSET.x;
    camera_transform_comp.position[1] = CAMERA_OFFSET.y;
    camera_transform_comp.position[2] = CAMERA_OFFSET.z;
    engineGameObjectUpdateTransformComponent(go_, &camera_transform_comp);
}

void project_c::CameraScript::late_update(float dt)
{
    engine::ScopedProfiler profiler("project_c::CameraScript::late_update");
    if (!find_player())
    {
        return;
    }
    const float delta_seconds = std::min(dt / 1000.0f, MAX_DELTA_SECONDS);

    const auto player_tc = engineGameObjectGetTransformComponent(player_go_);
    const glm::vec3 player_position(player_tc.position[0], player_tc.position[1], player_tc.position[2]);

    const glm::vec3 look_ahead_target = compute_look_ahead(player_position);
    look_ahead_ = smooth_damp(look_ahead_, look_ahead_target, look_ahead_velocity_, glm::vec3(LOOK_AHEAD_SMOOTH_TIME), delta_seconds);
    focus_ = smooth_damp(focus_, player_position + look_ahead_, focus_velocity_,
        glm::vec3(FOLLOW_SMOOTH_TIME_HORIZONTAL, FOLLOW_SMOOTH_TIME_VERTICAL, FOLLOW_SMOOTH_TIME_HORIZONTAL), delta_seconds);

    glm::vec3 shake_offset(0.0f);
    if (shake_remaining_seconds_ > 0.0f)
    {
        shake_elapsed_seconds_ += delta_seconds;
        shake_remaining_seconds_ = std::max(0.0f, shake_remaining_seconds_ - delta_seconds);
        const float fade = shake_remaining_seconds_ / shake_duration_seconds_;
        const float shake_x = std::sin(shake_elapsed_seconds_ * 83.0f) * shake_intensity_ * fade;
        const float shake_y = std::sin(shake_elapsed_seconds_ * 117.0f) * shake_intensity_ * 0.55f * fade;
        // along the screen axes (camera right and up)
        const glm::vec3 camera_forward = glm::normalize(-CAMERA_OFFSET);
        const glm::vec3 camera_right(1.0f, 0.0f, 0.0f);
        const glm::vec3 camera_up = glm::cross(camera_right, camera_forward);
        shake_offset = camera_right * shake_x + camera_up * shake_y;
        if (shake_remaining_seconds_ <= 0.0f)
        {
            shake_intensity_ = 0.0f;
            shake_duration_seconds_ = 0.0f;
        }
    }
    apply_camera_transform(shake_offset);

    update_occluders_fade(player_position, delta_seconds);
}

void project_c::CameraScript::snap_to_player()
{
    if (!find_player())
    {
        return;
    }
    const auto player_tc = engineGameObjectGetTransformComponent(player_go_);
    focus_ = glm::vec3(player_tc.position[0], player_tc.position[1], player_tc.position[2]);
    focus_velocity_ = glm::vec3(0.0f);
    look_ahead_ = glm::vec3(0.0f);
    look_ahead_velocity_ = glm::vec3(0.0f);
    apply_camera_transform(glm::vec3(0.0f));
}

bool project_c::CameraScript::find_player()
{
    if (player_go_ != ENGINE_INVALID_GAME_OBJECT_ID && engineGameObjectIsValid(player_go_))
    {
        return true;
    }
    const auto all_players = utils::get_game_objects_with_name("player");
    player_go_ = all_players.empty() ? ENGINE_INVALID_GAME_OBJECT_ID : all_players.front();
    return player_go_ != ENGINE_INVALID_GAME_OBJECT_ID;
}

glm::vec3 project_c::CameraScript::compute_look_ahead(const glm::vec3& player_position) const
{
    // cursor on the ground plane at the player's height
    const auto ray = utils::get_ray_from_mouse_position(go_);
    const glm::vec3 ray_from(ray.origin.x, ray.origin.y, ray.origin.z);
    const glm::vec3 ray_to(ray.direction.x, ray.direction.y, ray.direction.z);
    const glm::vec3 ray_direction = ray_to - ray_from;
    if (std::abs(ray_direction.y) < 0.0001f)
    {
        return glm::vec3(0.0f);
    }
    const float t = (player_position.y - ray_from.y) / ray_direction.y;
    const glm::vec3 cursor = ray_from + ray_direction * t;

    glm::vec3 offset = glm::vec3(cursor.x - player_position.x, 0.0f, cursor.z - player_position.z) * LOOK_AHEAD_FRACTION;
    const float length = glm::length(offset);
    if (length > LOOK_AHEAD_MAX_DISTANCE)
    {
        offset *= LOOK_AHEAD_MAX_DISTANCE / length;
    }
    return offset;
}

void project_c::CameraScript::apply_camera_transform(const glm::vec3& shake_offset)
{
    // orthographic camera: eye and target move together, otherwise the shake would be barely visible
    const glm::vec3 target = focus_ + shake_offset;
    auto camera_tc = engineGameObjectGetTransformComponent(go_);
    camera_tc.position[0] = target.x + CAMERA_OFFSET.x;
    camera_tc.position[1] = target.y + CAMERA_OFFSET.y;
    camera_tc.position[2] = target.z + CAMERA_OFFSET.z;
    engineGameObjectUpdateTransformComponent(go_, &camera_tc);

    auto camera_comp = engineGameObjectGetCameraComponent(go_);
    camera_comp.target[0] = target.x;
    camera_comp.target[1] = target.y;
    camera_comp.target[2] = target.z;
    engineGameObjectUpdateCameraComponent(go_, &camera_comp);
}

void project_c::CameraScript::update_occluders_fade(const glm::vec3& player_position, float delta_seconds)
{
    engine::ScopedProfiler profiler("project_c::CameraScript::update_occluders_fade");
    // everything a ray from the player toward the camera passes through. Engine ray returns only the closest hit,
    // so hits are collected by casting again with the previous hits ignored.
    std::unordered_set<engine_game_object_t> occluding(forced_fade_.begin(), forced_fade_.end());
    std::vector<engine_game_object_t> fade_targets;
    const glm::vec3 to_camera = CAMERA_OFFSET;
    for (const auto& sample_offset : OCCLUSION_SAMPLE_OFFSETS)
    {
        const glm::vec3 from = player_position + sample_offset;
        engine_ray_t ray{};
        ray.origin = { from.x, from.y, from.z };
        // engine ray "direction" is the end point of the ray
        ray.direction = { from.x + to_camera.x, from.y + to_camera.y, from.z + to_camera.z };

        std::vector<engine_game_object_t> ignore_list = { player_go_ };
        for (std::size_t i = 0; i < OCCLUDER_MAX_HITS_PER_RAY; i++)
        {
            const auto hit = enginePhysicsRayCast(ignore_list.data(), ignore_list.size(), &ray, CAMERA_DISTANCE);
            if (hit.go == ENGINE_INVALID_GAME_OBJECT_ID)
            {
                break;
            }
            ignore_list.push_back(hit.go);
            if (const auto* occluder = my_scene_->get_script<CameraOccluder>(hit.go))
            {
                fade_targets.clear();
                occluder->get_fade_targets(fade_targets);
                occluding.insert(fade_targets.begin(), fade_targets.end());
            }
        }
    }

    for (const auto go : occluding)
    {
        occluders_fade_.try_emplace(go, 0.0f);
    }

    for (auto it = occluders_fade_.begin(); it != occluders_fade_.end();)
    {
        const bool is_occluding = occluding.contains(it->first);
        const float target = is_occluding ? OCCLUDER_FADE : 0.0f;
        const float speed = OCCLUDER_FADE / (is_occluding ? OCCLUDER_FADE_OUT_SECONDS : OCCLUDER_FADE_IN_SECONDS);
        const float previous = it->second;
        const float step = speed * delta_seconds;
        it->second = previous < target ? std::min(previous + step, target) : std::max(previous - step, target);
        if (it->second != previous)
        {
            set_mesh_fade(it->first, it->second);
        }

        if (!is_occluding && it->second <= 0.0f)
        {
            it = occluders_fade_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void project_c::CameraScript::set_forced_fade(engine_game_object_t go, bool forced)
{
    if (forced)
    {
        forced_fade_.insert(go);
    }
    else
    {
        forced_fade_.erase(go);
    }
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
