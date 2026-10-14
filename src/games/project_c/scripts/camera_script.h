#pragma once
#include "base_script.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

namespace project_c
{

// Scripts which can block the camera's view of the player. The camera tells them when they do,
// they decide how to get out of the way (i.e. walls sink down, roof hides).
class CameraOccluder
{
public:
    virtual ~CameraOccluder() = default;
    // Game object whose CameraOccluder reacts when this one hides the player.
    // Usually its own, but i.e. invisible window colliders forward it to the visible window piece.
    virtual engine_game_object_t get_occlusion_owner() const = 0;
    // Called every frame while the occluder hides the player (occluding = true) and once when it stops.
    // player_feet_y lets i.e. walls sink exactly to the player's height.
    virtual void set_occluding(bool occluding, float player_feet_y) = 0;
};

class CameraScript : public BaseNode
{
public:
    CameraScript(engine::IScene* my_scene);
    void snap_to_player();
    void late_update(float dt) override;
    void shake(float intensity, float duration_seconds);
    // Keeps an occluder (its owner game object) occluding regardless of the rays, i.e. front wall of the room the player is in.
    void set_forced_occluding(engine_game_object_t go, bool forced);

private:
    bool find_player();
    glm::vec3 compute_look_ahead(const glm::vec3& player_position) const;
    void apply_camera_transform(const glm::vec3& shake_offset);
    void update_occluders(const glm::vec3& player_position, float delta_seconds);

    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    // point the camera looks at, follows player + look-ahead with a spring
    glm::vec3 focus_{ 0.0f };
    glm::vec3 focus_velocity_{ 0.0f };
    glm::vec3 look_ahead_{ 0.0f };
    glm::vec3 look_ahead_velocity_{ 0.0f };

    // occluder owner -> seconds since it last hid the player
    std::unordered_map<engine_game_object_t, float> occluders_;
    std::unordered_set<engine_game_object_t> forced_occluders_;

    float shake_remaining_seconds_ = 0.0f;
    float shake_duration_seconds_ = 0.0f;
    float shake_elapsed_seconds_ = 0.0f;
    float shake_intensity_ = 0.0f;
};

}
