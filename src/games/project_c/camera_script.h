#pragma once
#include "iscript.h"
#include <algorithm>

#include <glm/glm.hpp>

namespace project_c
{

class CameraScript : public engine::IScript
{
public:
    CameraScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        auto nc = engineSceneAddNameComponent(scene, go_);
        std::strncpy(nc.name, "camera", strlen("camera"));
        engineSceneUpdateNameComponent(scene, go_, &nc);

        auto camera_comp = engineSceneAddCameraComponent(scene, go_);
        camera_comp.enabled = true;
        camera_comp.clip_plane_near = 0.1f;
        camera_comp.clip_plane_far = 1000.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        camera_comp.type_union.perspective_fov = 45.0f;

        camera_comp.target[0] = 0.0f;
        camera_comp.target[1] = 0.0f;
        camera_comp.target[2] = 0.0f;

        engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene, go_);
        camera_transform_comp.position[0] = -2.273151f;
        camera_transform_comp.position[1] = 4.3652916f;
        camera_transform_comp.position[2] = 1.3330482f;
        engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);

        sc_ = get_spherical_coordinates(camera_transform_comp.position);
    }

    void update(float dt) override
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        const auto mouse_coords = engineApplicationGetMouseCoords(app);

        const auto dx = mouse_coords.x - mouse_coords_prev_.x;
        const auto dy = mouse_coords.y - mouse_coords_prev_.y;

        if (mouse_coords.x != mouse_coords_prev_.x || mouse_coords.y != mouse_coords_prev_.y)
        {
            mouse_coords_prev_ = mouse_coords;
        }

        const float move_speed = 1.0f * dt;

        if (engineApplicationIsMouseButtonDown(app, engine_mouse_button_t::ENGINE_MOUSE_BUTTON_LEFT))
        {
            if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_LSHIFT))
            {
                // strafe left/right adn top/down with left mouse button + left shift
                strafe(dx * move_speed, dy * move_speed);
            }
            else
            {
                rotate({ dx * move_speed, dy * move_speed });
            }
        }

        if (engineApplicationIsMouseButtonDown(app, engine_mouse_button_t::ENGINE_MOUSE_BUTTON_RIGHT))
        {
            // zoom in/out with right mouse button 
            // update spherical coordinates (radius -> sc_[0])
            translate({ 0.0f, 0.0f, dy * move_speed });
        }
    }

private:
    inline void translate(const glm::vec3& delta)
    {
        const auto scene = my_scene_->get_handle();
        // Decrease the radius based on the delta's z value
        sc_[0] -= delta.z;
        // Make sure the radius doesn't go below a certain threshold to prevent the camera from going inside the target
        sc_[0] = std::max(sc_[0], 0.1f);
        // Update the camera's position based on the new spherical coordinates
        const auto new_position = get_cartesian_coordinates(sc_);
        auto tc = engineSceneGetTransformComponent(scene, go_);
        auto cc = engineSceneGetCameraComponent(scene, go_);
        tc.position[0] = new_position[0] + cc.target[0];
        tc.position[1] = new_position[1] + cc.target[1];
        tc.position[2] = new_position[2] + cc.target[2];
        engineSceneUpdateTransformComponent(scene, go_, &tc);
    }

    inline void rotate(const glm::vec2 delta)
    {
        const auto scene = my_scene_->get_handle();
        // https://nerdhut.de/2020/05/09/unity-arcball-camera-spherical-coordinates/
        if (delta.x != 0 || delta.y != 0)
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            // Rotate the camera left and right
            sc_[1] += delta.x;

            // Rotate the camera up and down
            // Prevent the camera from turning upside down (1.5f = approx. Pi / 2)
            sc_[2] = std::clamp(sc_[2] + delta.y, -1.5f, 1.5f);

            const auto new_position = get_cartesian_coordinates(sc_);
            auto cc = engineSceneGetCameraComponent(scene, go_);
            tc.position[0] = new_position[0] + cc.target[0];
            tc.position[1] = new_position[1] + cc.target[1];
            tc.position[2] = new_position[2] + cc.target[2];
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
    }

    inline void strafe(float delta_x, float delta_y)
    {
        const auto scene = my_scene_->get_handle();
        // Get the current camera orientation
        auto tc = engineSceneGetTransformComponent(scene, go_);
        auto cc = engineSceneGetCameraComponent(scene, go_);

        // Compute the right vector from the camera's orientation
        glm::vec3 forward(cc.target[0] - tc.position[0], cc.target[1] - tc.position[1], cc.target[2] - tc.position[2]);
        glm::vec3 up(0.0f, 1.0f, 0.0f); // Assuming the up vector is (0, 1, 0)
        glm::vec3 right = glm::normalize(glm::cross(forward, up));

        // Update the camera's position
        tc.position[0] += delta_x * right.x;
        tc.position[1] += delta_x * right.y + delta_y;
        tc.position[2] += delta_x * right.z;

        // Update the camera's target
        cc.target[0] += delta_x * right.x;
        cc.target[1] += delta_x * right.y + delta_y;
        cc.target[2] += delta_x * right.z;

        // Update the transform and camera components
        engineSceneUpdateTransformComponent(scene, go_, &tc);
        engineSceneUpdateCameraComponent(scene, go_, &cc);
    }

    inline auto get_spherical_coordinates(const auto& cartesian)
    {
        const float r = std::sqrt(
            std::pow(cartesian[0], 2) +
            std::pow(cartesian[1], 2) +
            std::pow(cartesian[2], 2)
        );


        float phi = std::atan2(cartesian[2] / cartesian[0], cartesian[0]);
        const float theta = std::acos(cartesian[1] / r);

        if (cartesian[0] < 0)
            phi += 3.1415f;

        std::array<float, 3> ret{ 0.0f };
        ret[0] = r;
        ret[1] = phi;
        ret[2] = theta;
        return ret;
    }

    inline auto get_cartesian_coordinates(const auto& spherical)
    {
        std::array<float, 3> ret{ 0.0f };

        ret[0] = spherical[0] * std::cos(spherical[2]) * std::cos(spherical[1]);
        ret[1] = spherical[0] * std::sin(spherical[2]);
        ret[2] = spherical[0] * std::cos(spherical[2]) * std::sin(spherical[1]);

        return ret;
    }

private:
    std::array<float, 3> sc_;  // {radius, phi, theta}
    engine_coords_2d_t mouse_coords_prev_{};
};

}  // namespace project_c
