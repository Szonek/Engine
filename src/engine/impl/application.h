#pragma once
#include <engine.h>

#include "game_timer.h"
#include "graphics.h"

namespace engine
{
class Application
{
public:
    Application(const engine_application_create_desc_t& desc, engine_result_code_t& out_code);
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = default;
    Application& operator=(Application&&) = default;
    ~Application();

    engine_result_code_t run_scene(class Scene* scene, float delta_time);

    engine_application_frame_begine_info_t begine_frame();
    engine_application_frame_end_info_t end_frame();

    bool keyboard_is_key_down(engine_keyboard_keys_t key) const;

    engine_mouse_coords_t mouse_get_coords() const;
    bool mouse_is_button_down(engine_mouse_button_t button) const;

private:
    RenderContext rdx_;
    GameTimer timer_;

    Shader shader_;
    Geometry geometry_;

    Texture2D texture_container_;
    Texture2D texture_face_;
};

}  // namespace engine