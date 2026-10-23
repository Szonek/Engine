#pragma once
#include "utils.h"
#include "engine.h"

#include "scene_manager.h"
#include "script_registry.h"

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
    void update_scenes(float dt);

    // the script types scene files and the editor can put on game objects (see ScriptRegistry)
    ScriptRegistry& scripts() { return scripts_; }

private:
    engine_application_t app_handle_;
    ScriptRegistry scripts_;
    SceneManager scene_manager_;
};


} // namespace engine