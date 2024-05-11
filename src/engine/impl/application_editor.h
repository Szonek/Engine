#include "application.h"
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
    CameraContext camera_context_;
    bool editor_controlling_scene_ = false;
};

} // namespace engine