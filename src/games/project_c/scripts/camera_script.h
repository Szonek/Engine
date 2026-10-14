#pragma once
#include "base_script.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

namespace project_c
{

// Scripts which can block the camera's view of the player. Their mesh is faded out (dithered) while it does.
class CameraOccluder
{
public:
    virtual ~CameraOccluder() = default;
    // game objects with the meshes to fade (collider and mesh can live on different game objects,
    // one occluder can also take others with it, i.e. roof and the props standing on it)
    virtual void get_fade_targets(std::vector<engine_game_object_t>& out) const = 0;
};

class CameraScript : public BaseNode
{
public:
    CameraScript(engine::IScene* my_scene);
    void snap_to_player();
    void late_update(float dt) override;
    void shake(float intensity, float duration_seconds);
    // Keeps the mesh faded as if it was hiding the player, i.e. front wall of the room the player is in.
    void set_forced_fade(engine_game_object_t go, bool forced);

private:
    bool find_player();
    glm::vec3 compute_look_ahead(const glm::vec3& player_position) const;
    void apply_camera_transform(const glm::vec3& shake_offset);
    void update_occluders_fade(const glm::vec3& player_position, float delta_seconds);

    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    // point the camera looks at, follows player + look-ahead with a spring
    glm::vec3 focus_{ 0.0f };
    glm::vec3 focus_velocity_{ 0.0f };
    glm::vec3 look_ahead_{ 0.0f };
    glm::vec3 look_ahead_velocity_{ 0.0f };

    // fade target -> current fade
    std::unordered_map<engine_game_object_t, float> occluders_fade_;
    std::unordered_set<engine_game_object_t> forced_fade_;

    float shake_remaining_seconds_ = 0.0f;
    float shake_duration_seconds_ = 0.0f;
    float shake_elapsed_seconds_ = 0.0f;
    float shake_intensity_ = 0.0f;
};

}
