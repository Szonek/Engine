#include "iapplication.h"

#include <stdexcept>
#include <fmt/format.h>

engine::IApplication::IApplication(engine_application_create_desc_t create_desc)
    : scene_manager_(this)
{
    engine_result_code_t engine_error_code = engineApplicationCreate(&app_handle_, create_desc);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        throw std::runtime_error("Failed to create application");
    }
    engineApplicationSetActive(app_handle_);
}

engine::IApplication::~IApplication()
{
    scene_manager_.unregister_all_scenes();
    engineApplicationSetActive(nullptr);
    engineApplicationDestroy(app_handle_);
}

bool engine::IApplication::is_editor_enabled() const
{
    return engineEditorIsEnabled();
}

void engine::IApplication::update_scenes(float dt)
{
    scene_manager_.update(dt);
}

