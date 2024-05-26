#pragma once
#include "iscript.h"
#include "../prefab.h"

namespace project_c
{
class BaseNode : public engine::IScript
{
protected:
    BaseNode(engine::IScene* my_scene, std::string_view name);
    BaseNode(engine::IScene* my_scene, engine_game_object_t go, const AnimationController& anim_c, std::string_view name);
    BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name);
    BaseNode(engine::IScene* my_scene, const PrefabResult& pr, std::string_view name);

protected:
    AnimationController anim_controller_;
};




} // namespace project_c