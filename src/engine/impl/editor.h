#include "graphics.h"
#include "engine.h"

namespace engine
{

class Editor
{
public:
    Editor() = default;
    Editor(SDL_Window* window, SDL_GLContext gl_ctx);

    Editor(const Editor&) = delete;
    Editor(Editor&& rhs) noexcept;
    Editor& operator=(const Editor&) = delete;
    Editor& operator=(Editor&& rhs) noexcept;
    ~Editor();

    void begin_frame();

    void render_scene_hierarchy(class Scene* scene);
    void end_frame();

    void handle_event(SDL_Event& ev);
    bool wants_to_capture_mouse();

private:
    bool is_enabled_ = false;
};
} // namespace engine