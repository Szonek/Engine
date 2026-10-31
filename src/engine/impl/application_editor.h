#include "application.h"
#include "imgui/imgui.h"
#include "asset_store.h"
#include "file_watcher.h"
#include "editor_commands.h"
#include "editor_asset_panel.h"
#include "editor_camera.h"
#include "editor_modal_transform.h"
#include "render/outline_feature.h"
#include "render/grid_feature.h"

#include "components/name_component.h"

#include <entt/entt.hpp>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

    // Renames in place, like Unity's, Unreal's and Godot's hierarchies (F2, a double-click, the Rename menu item): the
    // game object's row becomes a text field holding its name; Enter or clicking away renames it (one undo step),
    // Escape gives the name back. A game object without a name component can't be renamed.
    void request_rename(const engine::Scene* scene, entt::entity e);
    bool is_renaming(entt::entity e) const { return renaming_ != entt::null && renaming_ == e; }
    std::array<char, ENGINE_ENTITY_NAME_MAX_LENGTH>& get_rename_buffer() { return rename_buffer_; }
    // true once after request_rename: the text field takes the keyboard focus
    bool take_rename_focus() { return std::exchange(rename_focus_, false); }
    // the frames in which the text field gets the focus it asked for
    bool is_rename_starting() const;
    void end_rename() { renaming_ = entt::null; }

private:
    IAssetSelection& assets_;
    // the selected game object's handle, found again from selected_ref_ by resolve_selection
    entt::entity selected_ = entt::null;
    GameObjectRef selected_ref_;
    bool force_open_selected_parents_ = false;
    entt::entity delete_without_undo_ = entt::null;
    entt::entity renaming_ = entt::null;
    std::array<char, ENGINE_ENTITY_NAME_MAX_LENGTH> rename_buffer_{};
    bool rename_focus_ = false;
    int rename_frame_ = 0;
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
    void on_play_mode_enter(class Scene* scene) override;
    void on_play_mode_exit(class Scene* scene) override;
    // closing the window (or the game's quit) with unsaved changes asks to save them first
    bool on_quit_requested() override;

private:
    // Play or Stop asked for by the toolbar or a shortcut, done at the start of the next frame
    enum class PlayModeRequest
    {
        eNone,
        ePlay,
        eStop,
    };
    // The File menu's commands (application_editor_scene_files.cpp), like Unity's: Save writes the scene to its file
    // (Save As when it has none), Open replaces the scene with a file, New with a template (a scene without a file).
    enum class SceneFileAction
    {
        eNone,
        eSave,
        eOpen,
        eNew,
        eQuit,
    };
    struct scene_file_request_t
    {
        SceneFileAction action = SceneFileAction::eNone;
        // eSave: the file, eOpen: the scene file, eNew: the template (asset IDs: paths under assets/scenes)
        std::string path;
    };
    // the dialog of Save As, Open and New
    enum class SceneFileDialog
    {
        eNone,
        eSaveAs,
        eOpen,
        eNew,
    };
    static constexpr ImVec4 PLAYING_TINT{ 0.10f, 0.22f, 0.36f, 1.0f };
    static constexpr ImVec4 PAUSED_TINT{ 0.30f, 0.24f, 0.08f, 1.0f };

    // the panel showing the scene image
    void render_scene_view();
    // Ctrl+P, Ctrl+Shift+P, Ctrl+Alt+P
    void handle_play_mode_shortcuts();
    void render_editor_controls(class Scene* scene, float dt);
    void render_scene_hierarchy_panel(class Scene* scene, float dt);
    // the asset browser, in the bottom panel
    void render_asset_browser_panel(class Scene* scene);
    // a model or scene file dragged from the asset browser and dropped on the scene view is placed where the mouse ray
    // hits a collider, or the ground (CommandPlacePrefab)
    void handle_asset_drop(class Scene* scene);
    // asks to confirm deleting a game object undo can't bring back (SceneHierarchyContext::request_delete_without_undo)
    void display_delete_without_undo_popup(class Scene* scene);
    // the File menu, in the toolbar's menu bar, and its shortcuts (Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S)
    void render_file_menu(class Scene* scene);
    void handle_file_shortcuts(class Scene* scene);
    // the Save As / Open / New dialog and the unsaved changes prompt
    void render_scene_file_dialogs(class Scene* scene);
    void render_scene_file_dialog(class Scene* scene);
    void render_unsaved_changes_prompt(class Scene* scene);
    void open_scene_file_dialog(SceneFileDialog dialog, class Scene* scene);
    // Save: Save As when the scene has no file. Open and New first ask to save unsaved changes (Save / Don't save /
    // Cancel). Only in Edit mode, like Unity: nothing of a play session is saved.
    void request_scene_file_action(class Scene* scene, SceneFileAction action, std::string path = {});
    // what waited for the prompt goes on (saved or discarded)
    void continue_after_prompt();
    // the File menu's request, at the start of the frame (on_frame_begine)
    void apply_scene_file_request();
    // what the editor holds of the scene's game objects (gizmo, modal transform), which are gone (Stop, Open, New)
    void forget_game_objects();
    // "<title> - <scene file>*" while there are unsaved changes
    void update_window_title(class Scene* scene);
    // the scene's file as the toolbar, the title and the prompt show it ("Untitled" without one)
    static std::string scene_display_name(const class Scene& scene);
    void render_entity_properties_panel(class Scene* scene, float dt);
    // the selected game object's name, at the top of the properties panel (like Unity's inspector header)
    void display_name_field(class Scene* scene, entt::entity entity);
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
    PlayModeRequest play_mode_request_ = PlayModeRequest::eNone;
    // the scene Play and Stop act on: the one the toolbar was last drawn for, none once it is released
    class Scene* play_mode_scene_ = nullptr;
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
    // the properties panel's name field: the text being edited while it is active, the game object's name otherwise
    // and the game object it is the name of (the selection may change before the field gives up the focus)
    std::array<char, ENGINE_ENTITY_NAME_MAX_LENGTH> name_field_buffer_{};
    entt::entity name_field_entity_ = entt::null;
    bool name_field_active_ = false;
    // the hierarchy panel has the keyboard focus (F2 renames the selection there and in the scene view)
    bool hierarchy_focused_ = false;
    // a Save, Open or New done at the start of the next frame, like Play and Stop: never during a scene update (the
    // editor camera has the game's cameras disabled then, a save would keep them so)
    scene_file_request_t scene_file_request_;
    // what waits for the unsaved changes prompt (Open, New, quit), or for the Save As the prompt's Save opened
    scene_file_request_t after_prompt_;
    bool open_unsaved_prompt_ = false;
    SceneFileDialog scene_file_dialog_ = SceneFileDialog::eNone;
    bool open_scene_file_dialog_popup_ = false;
    // the dialog's folder (relative to assets/, "scenes" or under it), name typed (Save As) and file picked (Open, New)
    std::string scene_file_dialog_folder_ = "scenes";
    std::array<char, 128> scene_file_dialog_name_{};
    std::string scene_file_dialog_picked_;
    // read from disk when the folder or the save path changes, not every frame: the folder's entries as listed, and
    // whether the checked save path exists and is a file the scene places
    std::optional<std::string> scene_file_dialog_listed_folder_;
    std::vector<asset_entry_t> scene_file_dialog_entries_;
    std::optional<std::string> scene_file_dialog_checked_path_;
    bool scene_file_dialog_path_exists_ = false;
    bool scene_file_dialog_path_placed_ = false;
    // the window's title as the game created it, and as it is shown
    std::string window_title_base_;
    std::string window_title_;
};

} // namespace engine