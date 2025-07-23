#include "iscript.h"
#include "iscene.h"

engine::IScript::IScript(engine::IScene* my_scene, engine_game_object_t go)
    : my_scene_(my_scene)
    , go_(go)
{}

engine::IScript::IScript(engine::IScene *my_scene)
    : IScript(my_scene, engineCreateGameObject())
{}

engine::IScript::~IScript()
{
    engineDestroyGameObject(go_);
}