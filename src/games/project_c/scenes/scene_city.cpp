#include "scene_city.h"

#include "../app.h"
#include "../scripts.h"

project_c::CityScene::CityScene(engine::IApplication* app)
    : IScene(app)
{
    auto camera_script = register_script<CameraScript>();

    auto typed_app = dynamic_cast<AppProjectC*>(app);
    typed_app->instantiate_prefab<project_c::Solider>(project_c::PREFAB_TYPE_SOLIDER, this);
}