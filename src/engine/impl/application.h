#pragma once
#include <engine.h>

#include "game_timer.h"
#include "graphics.h"
#include "ui_manager.h"
#include "ui_document.h"
#include "named_atlas.h"
#include "mesh_defs.h"
#include "skin.h"
#include "animation_controller.h"
#include "prefab.h"
#include "data_asset.h"
#include "play_mode.h"
#include "serialization/type_registry.h"
#include "render/renderer.h"

#include <atomic>
#include <set>
#include <vector>


namespace engine
{
class Application
{
public:
    // rdx must be valid (RenderContext::is_valid)
    Application(const engine_application_create_desc_t& desc, RenderContext&& rdx, engine_result_code_t& out_code);
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    virtual ~Application();

    virtual class Scene* allocate_scene(const engine_scene_create_desc_t& desc);
    virtual void release_scene(class Scene* scene);

    virtual engine_result_code_t update_scene(class Scene* scene, float delta_time);
    virtual engine_application_frame_begine_info_t begine_frame();
    virtual engine_application_frame_end_info_t end_frame();
    // Asks to close the application (engineApplicationRequestQuit, the window's close button): the start of a following
    // frame reports ENGINE_EVENT_QUIT, unless on_quit_requested holds it.
    void request_quit();

    virtual std::uint32_t add_texture(const TextureDesc& desc);
    virtual std::uint32_t add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t color_space);
    // An image in its own file under assets/models, named by its path there (like the image files of models).
    // Loading an image which is loaded already returns it with one more reference.
    virtual std::uint32_t load_texture_file(std::string_view path);
    virtual std::uint32_t get_texture(std::string_view name) const;
    virtual const Texture2D* get_texture(std::uint32_t idx) const;
    // the white texture drawn for a material without one
    engine_texture2d_t get_default_texture() const { return default_texture_idx_; }
    // asset ID of the texture, "" for ENGINE_INVALID_OBJECT_HANDLE
    virtual std::string get_texture_name(std::uint32_t idx) const;
    virtual std::vector<std::pair<std::uint32_t, std::string>> get_texture_names() const;
    // assets are reference counted (see Atlas): adding one gives the caller a reference, the last release destroys it
    virtual bool acquire_texture(std::uint32_t idx);
    virtual void release_texture(std::uint32_t idx);

    virtual bool add_font_from_file(std::string_view file_name, std::string_view handle_name);

    virtual std::uint32_t add_geometry(const engine_vertex_attributes_layout_t& verts_layout, std::int32_t vertex_count, std::span<const std::byte> verts_data, std::span<const uint32_t> inds, std::string_view name);
    virtual std::uint32_t get_geometry(std::string_view name) const;
    virtual std::string get_geometry_name(std::uint32_t idx) const;
    virtual std::vector<std::pair<std::uint32_t, std::string>> get_geometry_names() const;
    virtual const Geometry* get_geometry(std::uint32_t idx) const;
    virtual void release_geometry(std::uint32_t idx);

    // skin assets, named by the desc's asset ID
    virtual std::uint32_t add_skin(const SkinDesc& desc, const ModelNodeDesc& root);
    virtual std::uint32_t get_skin(std::string_view name) const;
    virtual const Skin* get_skin(std::uint32_t idx) const;
    virtual std::string get_skin_name(std::uint32_t idx) const;
    virtual std::vector<std::pair<std::uint32_t, std::string>> get_skin_names() const;
    virtual void release_skin(std::uint32_t idx);

    // animation set assets: a model's clips compiled for one skin, shared by that skin's controllers
    virtual std::uint32_t add_animation_set(std::string_view name, std::shared_ptr<const AnimationLibrary> animation_set);
    virtual std::uint32_t get_animation_set(std::string_view name) const;
    virtual std::shared_ptr<const AnimationLibrary> get_animation_set(std::uint32_t idx) const;
    virtual std::string get_animation_set_name(std::uint32_t idx) const;
    virtual void release_animation_set(std::uint32_t idx);

    // prefab assets: a model file with the assets created from it, named by its path relative to the models folder
    // ("characters/Rogue.glb"), or a scene file, named by its path relative to the scenes folder ("rooms/room_a.json",
    // see is_scene_prefab_id). Loading a file which is loaded already returns its prefab with one more reference.
    virtual std::uint32_t load_prefab(std::string_view path);
    virtual bool acquire_prefab(std::uint32_t idx);
    virtual void release_prefab(std::uint32_t idx);
    virtual std::uint32_t get_prefab(std::string_view name) const;
    virtual const Prefab* get_prefab(std::uint32_t idx) const;
    virtual std::string get_prefab_name(std::uint32_t idx) const;
    virtual std::vector<std::pair<std::uint32_t, std::string>> get_prefab_names() const;

    // data assets: a value of a game-defined struct type in a file under assets/data, named by its path there
    // ("enemies/skeleton.json"), see DataAsset. Loading a file which is loaded already returns it with one more reference.
    virtual std::uint32_t load_data_asset(std::string_view path);
    virtual void release_data_asset(std::uint32_t idx);
    virtual const DataAsset* get_data_asset(std::uint32_t idx) const;
    virtual std::string get_data_asset_name(std::uint32_t idx) const;
    virtual std::vector<std::pair<std::uint32_t, std::string>> get_data_asset_names() const;

    virtual std::uint32_t add_shader(const std::vector<std::string>& vertex_shader_name, const std::vector<std::string>& fragment_shader_name, std::string_view name);
    virtual std::uint32_t get_shader(std::string_view name) const;
    virtual std::string get_shader_name(std::uint32_t idx) const;
    virtual void release_shader(std::uint32_t idx);

    // play mode (see enginePlayModeGetState); Enter saves `scene` and the end of a Stop loads it back into `scene`: the
    // active one, which may be another scene than at Enter (the game replaced its scene while playing). Enter and Exit
    // are refused (logged) during a scene update, when the editor has changed the scene for it (see update_scene).
    engine_play_mode_state_t get_play_mode_state() const { return play_mode_.get_state(); }
    // whether the game plays this frame (scripts tick)
    bool is_playing() const { return play_mode_.is_playing(); }
    std::uint32_t get_play_session_count() const { return play_mode_.get_session_count(); }
    engine_result_code_t enter_play_mode(class Scene& scene);
    engine_result_code_t set_play_mode_paused(bool paused);
    engine_result_code_t step_play_mode();
    // Stop: Exiting, the next frame is the play session's last (see PlayMode)
    engine_result_code_t exit_play_mode();
    // the frame which began in Exiting has ended: finish_exit_play_mode is due (engineFrameEnd)
    bool is_play_mode_exit_due() const { return play_mode_.is_exit_due(); }
    engine_result_code_t finish_exit_play_mode(class Scene& scene);

    // the types scene files hold (see serialization/scene_serializer.h)
    const TypeRegistry& get_type_registry() const { return types_; }
    // a type the game describes, see register_game_type; nullptr (logged) when the description has a mistake
    const TypeInfo* register_game_type(const engine_type_desc_t& desc, bool component);

    virtual UiDocument load_ui_document(std::string_view file_name);
    virtual UiDataHandle create_ui_document_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings);

    // Part of the window the scene is shown in, in window coordinates with the origin in the top left corner.
    // The whole window, unless the editor shows the scene in a panel. Cameras, the game UI and mouse coordinates use it.
    virtual viewport_t get_scene_viewport() const;

    virtual bool is_mouse_enabled() { return true; }
    virtual bool is_keyboard_enabled() { return true; }

    virtual bool keyboard_is_key_down(engine_keyboard_keys_t key);
    // the key went down since the start of the last frame (a key event, not a held key's repeat)
    bool keyboard_is_key_pressed(engine_keyboard_keys_t key) const;

    virtual engine_fvec2_t mouse_get_coords();
    // What the game sees: mouse_get_coords() while the mouse is enabled, otherwise (i.e. over an editor panel)
    // the last position it saw, so the cursor stays where it left the scene instead of jumping.
    engine_fvec2_t mouse_get_game_coords();
    virtual bool mouse_is_button_down(engine_mouse_button_t button);

    virtual std::array<engine_finger_info_t, 10> get_finger_info_events() const;

protected:
    virtual void on_frame_begine(const engine_application_frame_begine_info_t&) {}
    virtual void on_sdl_event(SDL_Event /*e*/) {}
    virtual void on_frame_end() {}
    virtual void on_scene_create(class Scene* /*scene*/) {}
    virtual void on_scene_release(class Scene* /*scene*/) {}
    virtual void on_scene_update_pre(class Scene* /*scene*/, float /*delta_time*/) {}
    virtual void on_scene_update_post(class Scene* /*scene*/, float /*delta_time*/) {}
    // a play session started (Play): `scene` was saved
    virtual void on_play_mode_enter(class Scene* /*scene*/) {}
    // a play session ended (Stop): `scene` holds the edit state again
    virtual void on_play_mode_exit(class Scene* /*scene*/) {}
    // a quit was asked for (request_quit); false holds it: whoever held it calls confirm_quit() later, or never (cancelled)
    virtual bool on_quit_requested() { return true; }
    // the next frame (or this one, during on_frame_begine) reports ENGINE_EVENT_QUIT
    void confirm_quit() { quit_confirmed_ = true; }

private:
    // reads the data assets whose files changed again
    void reload_changed_data_assets();
    // whether the loaded data asset references (directly or through others) one of the data assets being read
    bool references_loading_data_asset(std::uint32_t idx) const;

protected:
    RenderContext rdx_;
    GameTimer timer_;
    // see engine_application_create_desc_t::max_delta_time
    float max_delta_time_ = 1000.0f / 3.0f;

    engine_texture2d_t default_texture_idx_;
    engine_material_t default_material_;
    Atlas<Texture2D> textures_atlas_;
    Atlas<Geometry> geometries_atlas_;
    Atlas<Skin> skins_atlas_;
    Atlas<std::shared_ptr<const AnimationLibrary>> animation_sets_atlas_;
    Atlas<std::unique_ptr<Prefab>> prefabs_atlas_;
    Atlas<Shader> shader_atlas_;
    TypeRegistry types_;
    // set (on the watcher's thread) when a data asset's file changed, so the frame only looks at them then; before the
    // atlas, whose assets' watcher callbacks set it
    std::atomic<bool> data_assets_changed_ = false;
    // after the assets they reference and the types of their values
    Atlas<std::unique_ptr<DataAsset>> data_assets_atlas_;
    // the data asset files being read (loaded or read again), so one referencing itself (directly or not) is caught
    std::set<std::string, std::less<>> loading_data_assets_;

    UiManager ui_manager_;
    std::array<engine_finger_info_t, 10> finger_info_buffer;
    // the centre of the scene until the game gets the mouse
    engine_fvec2_t last_game_mouse_coords_{ 0.5f, 0.5f };
    // the keys pressed since the start of the frame (their key events)
    std::vector<engine_keyboard_keys_t> keys_pressed_;

    Renderer renderer_;
    // with the editor or play_mode_control, play sessions can be stopped
    PlayMode play_mode_;
    // inside update_scene's scene update, see enter_play_mode
    bool scene_update_running_ = false;
    // a quit nothing holds (any more): every frame from now on reports ENGINE_EVENT_QUIT
    bool quit_confirmed_ = false;

private:
    bool is_play_mode_change_allowed() const;
};

}  // namespace engine