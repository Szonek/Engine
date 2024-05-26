#include "scene_city.h"

#include "../app.h"
#include "../scripts.h"

project_c::CityScene::CityScene(engine::IApplication* app)
    : IScene(app)
{
    auto camera_script = register_script<CameraScript>();

    auto typed_app = dynamic_cast<AppProjectC*>(app);
    typed_app->instantiate_prefab<project_c::Solider>(project_c::PREFAB_TYPE_SOLIDER, this);

    for (std::int32_t i = -3; i < 3; i++)
    {
        for (std::int32_t j = -3; j < 3; j++)
        {
            typed_app->instantiate_prefab<project_c::Floor>(project_c::PREFAB_TYPE_FLOOR, this, i, j);
        }
    }
    register_script<MainLight>();
}