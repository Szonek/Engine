#include "scene_city.h"

#include "../app.h"
#include "../scripts.h"

project_c::CityScene::CityScene(engine::IApplication* app)
    : IScene(app)
{
    auto camera_script = register_script<CameraScript>();

    auto typed_app = dynamic_cast<AppProjectC*>(app);
    register_script<project_c::Solider>(typed_app->instantiate_prefab(project_c::PREFAB_TYPE_SOLIDER, this));

    for (std::int32_t i = -5; i < 5; i++)
    {
        for (std::int32_t j = -3; j < 3; j++)
        {
            register_script<project_c::Floor>(typed_app->instantiate_prefab(project_c::PREFAB_TYPE_FLOOR, this), i, j);
        }
    }
    register_script<MainLight>();
    register_script<SpotLight>();
}