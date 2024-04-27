#pragma once
#include <engine.h>

#include "game_timer.h"
#include "graphics.h"
#include "ui_manager.h"
#include "ui_document.h"
#include "editor.h"
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
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    virtual ~Application();

    virtual class Scene* create_scene(const engine_scene_create_desc_t& desc);
    virtual void release_scene(class Scene* scene);

    virtual engine_result_code_t update_scene(class Scene* scene, float delta_time);
    virtual engine_application_frame_begine_info_t begine_frame();
    virtual engine_application_frame_end_info_t end_frame();

    virtual std::uint32_t add_texture(const engine_texture_2d_create_desc_t& desc, std::string_view texture_name);
    virtual std::uint32_t add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t color_space);
    virtual std::uint32_t get_texture(std::string_view name) const;

    virtual std::uint32_t add_font_from_file(std::string_view file_name, std::string_view handle_name);
    virtual std::uint32_t get_font(std::string_view name) const;

    virtual std::uint32_t add_geometry(const engine_vertex_attributes_layout_t& verts_layout, std::int32_t vertex_count, std::span<const std::byte> verts_data, std::span<const uint32_t> inds, std::string_view name);
    virtual std::uint32_t get_geometry(std::string_view name) const;
    virtual const Geometry* get_geometry(std::uint32_t idx) const;

    virtual std::uint32_t add_material(const engine_material_create_desc_t& desc, std::string_view name);
    virtual std::uint32_t get_material(std::string_view name) const;

    virtual engine_model_desc_t load_model_desc_from_file(engine_model_specification_t spec, std::string_view name);
    virtual void release_model_desc(engine_model_desc_t* info);

    virtual UiDocument load_ui_document(std::string_view file_name);
    virtual UiDataHandle create_ui_document_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings);

    virtual bool keyboard_is_key_down(engine_keyboard_keys_t key);

    virtual engine_coords_2d_t mouse_get_coords();
    virtual bool mouse_is_button_down(engine_mouse_button_t button);

    virtual std::array<engine_finger_info_t, 10> get_finger_info_events() const;

protected:
    virtual void on_frame_begine() {}
    virtual void on_sdl_event(SDL_Event e) {}
    virtual void on_frame_end() {}
    virtual void on_scene_update(class Scene* scene, float delta_time) {}
    virtual bool is_mouse_enabled() { return true; }

protected:
    RenderContext rdx_;
    GameTimer timer_;

    template<typename T>
    class Atlas
    {
        static constexpr const size_t SIZE = 1024;
        using ArrayType = std::array<T, SIZE>;
    public:
        Atlas()
            : current_idx_(0)
        {
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

        const T* get_object(std::uint32_t idx) const
        {
            return &objects_[idx];
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
            return ENGINE_INVALID_OBJECT_HANDLE;
        }

        const ArrayType& get_objects_view() const { return objects_; }
    private:
        ArrayType objects_;
        std::array<std::string, SIZE> names_;
        std::uint32_t current_idx_;
    };

    Atlas<Texture2D> textures_atlas_;
    engine_texture2d_t default_texture_idx_;
    Atlas<Geometry> geometries_atlas_;;
    Atlas<engine_material_create_desc_t> materials_atlas_;
    UiManager ui_manager_;
    std::array<engine_finger_info_t, 10> finger_info_buffer;
};

}  // namespace engine