#pragma once

#include "iscene.h"

namespace project_c
{
class CityScene : public engine::IScene
{
public:
    CityScene(engine::IApplication* app);

    static constexpr const char* get_name() { return "CityScene"; }
private:
};

}// namespace project_c