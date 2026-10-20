#pragma once
#include "base_script.h"

#include <glm/glm.hpp>

namespace project_c
{

class CameraScript : public BaseNode
{
public:
    CameraScript(engine::IScene* my_scene);
    // snaps to the player
    void on_start() override;
    void snap_to_player();
    void late_update(float dt) override;
    void shake(float intensity, float duration_seconds);

private:
    bool find_player();
    glm::vec3 compute_look_ahead(const glm::vec3& player_position) const;
    void apply_camera_transform(const glm::vec3& shake_offset);

    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    // point the camera looks at, follows player + look-ahead with a spring
    glm::vec3 focus_{ 0.0f };
    glm::vec3 focus_velocity_{ 0.0f };
    glm::vec3 look_ahead_{ 0.0f };
    glm::vec3 look_ahead_velocity_{ 0.0f };

    float shake_remaining_seconds_ = 0.0f;
    float shake_duration_seconds_ = 0.0f;
    float shake_elapsed_seconds_ = 0.0f;
    float shake_intensity_ = 0.0f;
};

}
