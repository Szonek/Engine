#pragma once
#include <engine.h>

#include "game_timer.h"
#include "graphics.h"
#include "ui_manager.h"
#include "ui_document.h"
#include "named_atlas.h"
#include "nav_mesh.h"

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

    virtual std::uint32_t add_nav_mesh(std::string_view name);
    virtual std::uint32_t get_nav_mesh(std::string_view name) const;
    virtual const NavMesh* get_nav_mesh(std::uint32_t idx) const;

    virtual bool add_font_from_file(std::string_view file_name, std::string_view handle_name);

    virtual std::uint32_t add_geometry(const engine_vertex_attributes_layout_t& verts_layout, std::int32_t vertex_count, std::span<const std::byte> verts_data, std::span<const uint32_t> inds, std::string_view name);
    virtual std::uint32_t get_geometry(std::string_view name) const;
    virtual const Geometry* get_geometry(std::uint32_t idx) const;

    virtual std::uint32_t add_material(const engine_material_create_desc_t& desc, std::string_view name);
    virtual std::uint32_t get_material(std::string_view name) const;

    virtual engine_model_desc_t load_model_desc_from_file(engine_model_specification_t spec, std::string_view name, std::string_view base_dir);
    virtual void release_model_desc(engine_model_desc_t* info);

    virtual UiDocument load_ui_document(std::string_view file_name);
    virtual UiDataHandle create_ui_document_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings);

    virtual bool is_mouse_enabled() { return true; }
    virtual bool is_keyboard_enabled() { return true; }

    virtual bool keyboard_is_key_down(engine_keyboard_keys_t key);

    virtual engine_coords_2d_t mouse_get_coords();
    virtual bool mouse_is_button_down(engine_mouse_button_t button);

    virtual std::array<engine_finger_info_t, 10> get_finger_info_events() const;

protected:
    virtual void on_frame_begine(const engine_application_frame_begine_info_t&) {}
    virtual void on_sdl_event(SDL_Event e) {}
    virtual void on_frame_end() {}
    virtual void on_scene_create(class Scene* scene) {}
    virtual void on_scene_release(class Scene* scene) {}
    virtual void on_scene_update_pre(class Scene* scene, float delta_time) {}
    virtual void on_scene_update_post(class Scene* scene, float delta_time) {}

protected:
    RenderContext rdx_;
    GameTimer timer_;

    engine_texture2d_t default_texture_idx_;
    Atlas<Texture2D> textures_atlas_;
    Atlas<Geometry> geometries_atlas_;
    Atlas<NavMesh> nav_mesh_atlas_;
    Atlas<engine_material_create_desc_t> materials_atlas_;
    UiManager ui_manager_;
    std::array<engine_finger_info_t, 10> finger_info_buffer;
};

}  // namespace engine