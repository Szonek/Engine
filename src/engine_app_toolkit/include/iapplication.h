#pragma once
#include "utils.h"
#include "engine.h"

#include "play_session_tracker.h"
#include "scene_manager.h"
#include "script_registry.h"

#include <cstdint>
#include <string>

namespace engine
{

class ENGINE_APP_TOOLKIT_API IApplication
{
public:
    IApplication(engine_application_create_desc_t create_desc);
    IApplication(const IApplication& rhs) = delete;
    IApplication(IApplication&& rhs) noexcept = default;
    IApplication& operator=(const IApplication& rhs) = delete;
    IApplication& operator=(IApplication&& rhs) noexcept = default;
    virtual ~IApplication();

    template<typename T>
    IScene* register_scene()
    {
        return scene_manager_.register_scene<T>();
    }

    IScene* get_scene(std::string_view name)
    {
        return scene_manager_.get_scene(name);
    }

    void unregister_scene(std::string_view name)
    {
        scene_manager_.unregister_scene(name);
    }

    bool is_editor_enabled() const;
    // one frame of the scenes; a play session starting or ending (see IScene) calls on_play_begin / on_play_end first
    void update_scenes(float dt);

    // the script types scene files and the editor can put on game objects (see ScriptRegistry)
    ScriptRegistry& scripts() { return scripts_; }

protected:
    // A play session starts (Play, or the first frame of an application which starts playing), before any scene's
    // on_play_begin: remember the application's state a session may change (Unreal's GameInstance), Stop doesn't give
    // it back by itself.
    virtual void on_play_begin() {}
    // the play session ended (Stop), before the scenes' on_play_end: put that state back
    virtual void on_play_end() {}

private:
    engine_application_t app_handle_;
    PlaySessionTracker play_session_;
    ScriptRegistry scripts_;
    SceneManager scene_manager_;
};


} // namespace engine