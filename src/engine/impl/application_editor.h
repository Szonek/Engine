#include "application.h"

namespace engine
{

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
    void on_frame_begine() override;
    void on_sdl_event(SDL_Event e) override;
    void on_frame_end() override;
    void on_scene_update(class Scene* scene, float delta_time) override;
    bool is_mouse_enabled() override;
};

} // namespace engine