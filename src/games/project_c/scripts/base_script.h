#pragma once
#include "iscript.h"

namespace project_c
{

class BaseNode : public engine::IScript
{
protected:
    BaseNode(engine::IScene* my_scene, std::string_view name);
    BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name);
};




} // namespace project_c