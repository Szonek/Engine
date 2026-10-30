#include "application.h"
#include "asset_store.h"
#include "file_watcher.h"
#include "editor_commands.h"
#include "editor_asset_panel.h"
#include "editor_camera.h"
#include "editor_modal_transform.h"
#include "render/outline_feature.h"
#include "render/grid_feature.h"

#include <entt/entt.hpp>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

namespace engine
{
class ApplicationEditor;
class CameraScript
{
public:
    CameraScript() = default;
    CameraScript(Scene* scene, ApplicationEditor* app);

    void enable();
    void disable();
    // `selected` is what F frames (entt::null: nothing); without `accepts_input` the mouse and keys belong to something
    // else (the modal transform)
    void update(float dt, entt::entity selected, bool accepts_input);

    entt::entity get_entity() const { return go_; }
    EditorCamera& get_camera() { return camera_; }

private:
    // the bounding sphere of the entity and its children: their origins, at least about their size
    bool frame_entity(entt::entity entity);
    float get_fov_y() const;

private:
    Scene* my_scene_ = nullptr;
    ApplicationEditor* app_ = nullptr;
    entt::entity go_ = entt::null;
    EditorCamera camera_{ glm::vec3(-1.0f, 1.7f, 5.0f), glm::vec3(-2.0f, 0.1f, 0.2f) };
};

class SceneHierarchyContext
{
public:
    // `assets` (the asset browser's selection) must outlive the context
    explicit SceneHierarchyContext(IAssetSelection& assets) : assets_(assets) {}

    // selects the game object (entt::null for none) with an undo step; selecting one deselects the selected file
    void set_selected_entity(engine::Scene* scene, entt::entity e, CommandManager& commands);
    // nothing selected, neither a game object nor a file (a click on empty space), with an undo step
    void deselect_all(engine::Scene* scene, CommandManager& commands);
    entt::entity get_selected_entity() const;
    bool has_selected_entity() const;

    void set_forced_open_selected_parents(bool value);
    bool is_forced_open_selected_parents() const;
    void sync_selected_from_scene(engine::Scene* scene);
    // The selection refers to its game object by persistent ID (GameObjectRef): when the game object got a new handle
    // (Stop loaded the scene saved at Play back), the selection follows it and marks it again; when it is gone (a script
    // destroyed it), nothing is selected any more.
    void resolve_selection(engine::Scene* scene);
    // no game object selected, without an undo step: the scene it was in was released
    void reset();

    // deleting a game object undo can't bring back (a transient one) waits for the user to confirm it in a popup
    void request_delete_without_undo(entt::entity e);
    // the game object whose delete was requested since the last call, or entt::null
    entt::entity take_delete_without_undo_request();

private:
    IAssetSelection& assets_;
    // the selected game object's handle, found again from selected_ref_ by resolve_selection
    entt::entity selected_ = entt::null;
    GameObjectRef selected_ref_;
    bool force_open_selected_parents_ = false;
    entt::entity delete_without_undo_ = entt::null;
};

class ApplicationEditor : public Application
{
public:
    ApplicationEditor(const engine_application_create_desc_t& desc, RenderContext&& rdx, engine_result_code_t& out_code);
    ApplicationEditor(const ApplicationEditor&) = delete;
    ApplicationEditor& operator=(const ApplicationEditor&) = delete;
    ApplicationEditor(ApplicationEditor&&) = delete;
    ApplicationEditor& operator=(ApplicationEditor&&) = delete;
    ~ApplicationEditor();
    bool is_game_view() const;
    // The mouse is over the scene panel (or a button pressed over it is still held), not over another editor panel.
    bool is_mouse_over_scene() const;
    // the scene panel, or the whole window while ImGui is hidden (F1)
    viewport_t get_scene_viewport() const override;

protected:
    void on_frame_begine(const engine_application_frame_begine_info_t& frame_begin_info) override;
    void on_sdl_event(SDL_Event e) override;
    void on_frame_end() override;
    void on_scene_update_post(class Scene* scene, float delta_time) override;
    void on_scene_update_pre(class Scene* scene, float delta_time) override;
    bool is_mouse_enabled() override;
    bool is_keyboard_enabled() override;
    void on_scene_create(class Scene* scene) override;
    void on_scene_release(class Scene* scene) override;
    void on_play_mode_exit(class Scene* scene) override;

private:
    // the panel showing the scene image
    void render_scene_view();
    void render_editor_controls(class Scene* scene, float dt);
    void render_scene_hierarchy_panel(class Scene* scene, float dt);
    // the asset browser, in the bottom panel
    void render_asset_browser_panel(class Scene* scene);
    // a model or scene file dragged from the asset browser and dropped on the scene view is placed where the mouse ray
    // hits a collider, or the ground (CommandPlacePrefab)
    void handle_asset_drop(class Scene* scene);
    // asks to confirm deleting a game object undo can't bring back (SceneHierarchyContext::request_delete_without_undo)
    void display_delete_without_undo_popup(class Scene* scene);
    void render_entity_properties_panel(class Scene* scene, float dt);
    void render_debug_panel(class Scene* scene, float dt);
    void render_guizmo(class Scene* scene);
    void handle_mouse_picking(class Scene* scene);
    // Blender's G / R / S on the selected game object: starts, follows the mouse, confirms (one undo step) or cancels
    void handle_modal_transform(class Scene* scene);
    void draw_modal_transform(class Scene* scene);
    void cancel_modal_transform(class Scene* scene);

private:
    class CameraContext
    {
    public:
        void attach_scene(Scene* scene, ApplicationEditor* app);
        void detach_scene(Scene* scene);
        void on_scene_update_pre(Scene* scene, float dt, entt::entity selected, bool accepts_input);
        void on_scene_update_post(Scene* scene, float dt);
        bool is_enabled(engine::Scene* scene) const;
        EditorCamera* get_camera(engine::Scene* scene);
        entt::entity get_camera_entity(engine::Scene* scene) const;

    private:
        struct camera_data_t
        {
            bool is_enabled;
            CameraScript camera;
            std::vector<entt::entity> user_camera_entities_to_enable_back;
        };
        std::map<engine::Scene*, camera_data_t> cameras_;
    };


    class EditorWindowsContext
    {
    public:
        EditorWindowsContext() = default;

        bool is_initialized() const { return initalized_; }
        void initialize(std::uint32_t dockspace_id);

        const char* get_window_up() const { return window_up; }
        const char* get_window_down() const { return window_down; }
        const char* get_window_down_right() const { return window_down_right; }
        const char* get_window_left() const { return window_left; }
        const char* get_window_right() const { return window_right; }
        const char* get_window_scene() const { return window_scene; }

    private:
        bool initalized_ = false;

    private:
        static constexpr const char* window_up = "Up Window";
        static constexpr const char* window_down = "Down Window";
        static constexpr const char* window_down_right = "Down-Right Window";
        static constexpr const char* window_left = "Left Window";
        static constexpr const char* window_right = "Right Window";
        static constexpr const char* window_scene = "Scene Window";
    };

    enum class EditorView
    {
        eGame,
        eEditor,
    };

private:
    CameraContext camera_context_;
    // what the modal transform and the gizmo snap to
    editor_snap_steps_t snap_steps_;
    std::optional<ModalTransform> modal_transform_;
    entt::entity modal_transform_entity_ = entt::null;
    engine_transform_component_t modal_transform_original_{};
    // the gizmo handle under the cursor while no gizmo is dragged, and whose gizmo it is: the handle a drag starts on
    entt::entity gizmo_hovered_entity_ = entt::null;
    std::optional<ModalTransformMode> gizmo_hovered_handle_;
    // the click that confirmed or cancelled a modal transform doesn't pick, orbit or fly until it is released
    bool mouse_consumed_ = false;
    EditorView editor_view_ = EditorView::eGame;
    EditorWindowsContext editor_windows_context_;
    bool draw_guizmo_ = true;
    float frame_delta_time_ = 0.0f;
    // Play was pressed in Edit mode: the play session starts with the next scene update
    bool play_requested_ = false;
    CommandManager commands_;
    std::shared_ptr<OutlineFeature> outline_feature_;
    std::shared_ptr<GridFeature> grid_feature_;
    bool draw_grid_ = true;
    bool render_imgui_ = true;
    // the scene is rendered into it at its panel's size
    std::shared_ptr<Texture2D> scene_view_texture_;
    viewport_t scene_view_rect_{};
    bool scene_view_has_mouse_ = false;
    // a mouse button pressed over the scene panel is still held
    bool scene_view_mouse_held_ = false;
    bool scene_view_focused_ = false;
    // the properties panel shows the file selected in it while no game object is selected (selecting one deselects the
    // other); before the hierarchy context, which uses its selection
    AssetBrowserPanel asset_browser_{ *this };
    SceneHierarchyContext scene_hierarchy_context_{ asset_browser_ };
    // the game object whose delete without undo waits for its confirmation
    entt::entity delete_without_undo_ = entt::null;
};

} // namespace engine