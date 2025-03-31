#include "application.h"
#include "asset_store.h"
#include "file_watcher.h"
#include "material.h"
#include <entt/entt.hpp>

#include <map>
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
    void update(float dt);

    entt::entity get_entity() const { return go_; }

private:
    void translate(const glm::vec3& delta);
    void rotate(const glm::vec2 delta);
    void strafe(float delta_x, float delta_y);

private:
    Scene* my_scene_ = nullptr;
    ApplicationEditor* app_ = nullptr;
    entt::entity go_ = entt::null;
    std::array<float, 3> sc_;  // {radius, phi, theta}
    engine_coords_2d_t mouse_coords_prev_{};
};

struct OutlinePostProccessEffect
{
    MaterialStaticGeometryUnlit material_static_geometry_unlit;
    MaterialSkinnedGeometryUnlit material_skinned_geometry_unlit;
    ComputeShader compute_shader_edge_detection;
    Framebuffer fbo_outline;

    OutlinePostProccessEffect(std::size_t init_width, std::uint32_t init_height)
        : compute_shader_edge_detection({ "sobel_edge_detection.cs" })
        , fbo_outline(init_width, init_height, { DataLayout::eRGBA_U8 }, false)
    {
    }

};

class SceneHierarchyContext
{
public:
    void set_selected_entity(engine::Scene* scene, entt::entity e);
    entt::entity get_selected_entity() const;
    bool has_selected_entity() const;

    void set_forced_open_selected_parents(bool value);
    bool is_forced_open_selected_parents() const;

private:
    entt::entity selected_ = entt::null;
    bool force_open_selected_parents_ = false;
};

class ApplicationEditor : public Application
{
public:
    ApplicationEditor(const engine_application_create_desc_t& desc, engine_result_code_t& out_code);
    ApplicationEditor(const ApplicationEditor&) = delete;
    ApplicationEditor& operator=(const ApplicationEditor&) = delete;
    ApplicationEditor(ApplicationEditor&&) = delete;
    ApplicationEditor& operator=(ApplicationEditor&&) = delete;
    ~ApplicationEditor();

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

private:
    void render_editor_controls(class Scene* scene, float dt);
    void render_scene_hierarchy_panel(class Scene* scene, float dt);
    void render_entity_properties_panel(class Scene* scene, float dt);
    void render_debug_panel(class Scene* scene, float dt);
    void render_outline(class Scene* scene);
    void render_guizmo(class Scene* scene);
    void handle_mouse_picking(class Scene* scene);

private:
    class CameraContext
    {
    public:
        void attach_scene(Scene* scene, ApplicationEditor* app);
        void detach_scene(Scene* scene);
        void on_scene_update_pre(Scene* scene, float dt);
        void on_scene_update_post(Scene* scene, float dt);
        bool is_enabled(engine::Scene* scene) const;

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

        viewport_t get_scene_render_viewport() const;

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
    EditorView editor_view_ = EditorView::eGame;
    EditorWindowsContext editor_windows_context_;
    bool draw_guizmo_ = true;
    OutlinePostProccessEffect outline_effect_;
    SceneHierarchyContext scene_hierarchy_context_;
};

} // namespace engine