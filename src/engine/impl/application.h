#pragma once
#include <engine.h>

#include "game_timer.h"
#include "graphics.h"
#include "ui_manager.h"

#include <array>
#include <string>

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

    engine_result_code_t update_scene(class Scene* scene, float delta_time);
    engine_application_frame_begine_info_t begine_frame();
    engine_application_frame_end_info_t end_frame();

    std::uint32_t add_texture_from_memory(const engine_texture_2d_create_from_memory_desc_t& desc, std::string_view texture_name);
    std::uint32_t add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t color_space);
    std::uint32_t add_font_from_file(std::string_view file_name, std::string_view handle_name);
    std::uint32_t get_font(std::string_view name) const;

    std::uint32_t add_geometry_from_memory(std::span<const engine_vertex_attribute_t> verts, std::span<const uint32_t> inds, std::string_view name);
    std::uint32_t get_geometry(std::string_view name) const;

    engine_model_info_t load_model_info_from_file(engine_model_specification_t spec, std::string_view name);
    void release_model_info(engine_model_info_t* info);

    bool keyboard_is_key_down(engine_keyboard_keys_t key);

    engine_mouse_coords_t mouse_get_coords();
    bool mouse_is_button_down(engine_mouse_button_t button);

    std::span<const engine_finger_info_t> get_finger_info_events() const;
private:
    RenderContext rdx_;
    GameTimer timer_;

    template<typename T>
    class Atlas
    {
        static constexpr const size_t SIZE = 1024;
        using ArrayType = std::array<T, SIZE>;
    public:
        Atlas()
            : current_idx_(ENGINE_INVALID_GAME_OBJECT_ID)
        {
            // move to next so it dont point to invalid obj
            current_idx_++;
        }
        Atlas(const Atlas& rhs) = delete;
        Atlas(Atlas&& rhs) = delete;
        Atlas& operator=(const Atlas& rhs) = delete;
        Atlas& operator=(Atlas&& rhs) = delete;
        ~Atlas() = default;

        std::uint32_t add_object(std::string_view name, T&& t)
        {
            objects_[current_idx_] = std::move(t);
            names_[current_idx_] = name.data();
            return current_idx_++;
        }

        std::uint32_t get_object(std::string_view name) const
        {
            for (std::uint32_t i = 0; const auto & n : names_)
            {
                if (n.compare(name) == 0)
                {
                    return i;
                }
                i++;
            }
            return 0;
        }

        const ArrayType& get_objects_view() const { return objects_; }
    private:
        ArrayType objects_;
        std::array<std::string, SIZE> names_;
        std::uint32_t current_idx_;
    };

    Atlas<Texture2D> textures_atlas_;
    Atlas<Geometry> geometries_atlas_;
    UiManager ui_manager_;

    std::array<engine_finger_info_t, 10> finger_info_buffer;
};

}  // namespace engine