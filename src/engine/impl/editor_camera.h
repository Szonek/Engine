#pragma once
#include <glm/glm.hpp>

namespace engine
{
// What the editor camera reads in one frame. Collected by the editor (from ImGui), so the camera itself needs no window.
struct editor_camera_input_t
{
    glm::vec2 mouse_delta{ 0.0f };  // pixels, y down
    float wheel = 0.0f;  // notches, positive: away from the user
    bool lmb = false;
    bool mmb = false;
    bool rmb = false;
    bool alt = false;
    bool shift = false;
    bool ctrl = false;
    // fly mode (right mouse button held): WASD, Q down, E up
    bool move_forward = false;
    bool move_back = false;
    bool move_left = false;
    bool move_right = false;
    bool move_down = false;
    bool move_up = false;
};

// The editor's viewport camera, GL-free. A turntable around a pivot (Blender, Godot): eye = pivot - forward * distance.
//   orbit:  middle mouse drag, or Alt + left drag (Unity, Unreal, Blender's "emulate 3 button mouse")
//   pan:    Shift + middle drag; the point under the cursor follows it
//   zoom:   wheel, or Ctrl + middle drag; toward the pivot
//   fly:    hold right mouse button: mouse look, WASD move, Q/E down/up, Shift faster, wheel changes the fly speed
//           (Unity, Unreal, Godot)
//   frame:  frame() moves the pivot onto a bounding sphere, set_view() looks along an axis (numpad views); both glide
class EditorCamera
{
public:
    EditorCamera(const glm::vec3& eye, const glm::vec3& pivot);

    // dt in milliseconds; viewport_height in pixels; fov_y in radians (the perspective's vertical field of view)
    void update(const editor_camera_input_t& input, float dt, float viewport_height, float fov_y);

    // look at a sphere so it fills the view
    void frame(const glm::vec3& center, float radius, float fov_y);
    // look from an axis-aligned direction (yaw, pitch in radians) at the current pivot
    void set_view(float yaw, float pitch);

    glm::vec3 get_eye() const;
    glm::vec3 get_pivot() const { return pivot_; }
    glm::vec3 get_forward() const;
    glm::vec3 get_right() const;
    glm::vec3 get_up() const;
    float get_distance() const { return distance_; }
    float get_yaw() const { return yaw_; }
    float get_pitch() const { return pitch_; }

    bool is_flying() const { return flying_; }
    // meters per second, before Shift
    float get_fly_speed() const { return fly_speed_; }
    void set_fly_speed(float speed);

public:
    static constexpr float ORBIT_SENSITIVITY = 0.005f;  // radians per pixel
    static constexpr float LOOK_SENSITIVITY = 0.003f;  // radians per pixel
    static constexpr float WHEEL_ZOOM_FACTOR = 0.85f;  // distance multiplier per notch
    static constexpr float DRAG_ZOOM_SENSITIVITY = 0.01f;  // per pixel, exponential
    static constexpr float MIN_DISTANCE = 0.1f;
    static constexpr float MAX_DISTANCE = 5000.0f;
    static constexpr float MAX_PITCH = 1.5533f;  // 89 degrees: the view matrix uses the world up
    static constexpr float FLY_BOOST = 3.0f;  // Shift
    static constexpr float FLY_WHEEL_FACTOR = 1.25f;  // fly speed multiplier per notch
    static constexpr float MIN_FLY_SPEED = 0.1f;
    static constexpr float MAX_FLY_SPEED = 500.0f;
    static constexpr float TRANSITION_TIME = 200.0f;  // milliseconds

private:
    struct view_t
    {
        glm::vec3 pivot{ 0.0f };
        float yaw = 0.0f;
        float pitch = 0.0f;
        float distance = 1.0f;
    };
    void start_transition(const view_t& to);
    void update_transition(float dt);

private:
    glm::vec3 pivot_{ 0.0f };
    float yaw_ = 0.0f;  // 0: looking down -Z, positive turns right
    float pitch_ = 0.0f;  // positive looks up
    float distance_ = 1.0f;
    float fly_speed_ = 5.0f;
    bool flying_ = false;

    view_t transition_from_{};
    view_t transition_to_{};
    float transition_time_ = TRANSITION_TIME;  // >= TRANSITION_TIME: none running
};
}  // namespace engine
