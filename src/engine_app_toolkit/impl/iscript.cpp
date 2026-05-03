#include "iscript.h"
#include "iscene.h"

#include <format>

engine::IScript::IScript(engine::IScene* my_scene, engine_game_object_t go)
    : my_scene_(my_scene)
    , go_(go)
{}

engine::IScript::IScript(engine::IScene *my_scene)
    : IScript(my_scene, engineGameObjectCreate())
{}

engine::IScript::~IScript()
{
    if (engineGameObjectIsValid(go_))
    {
        engineGameObjectDestroy(go_);
    }
    else
    {
        engineLog(std::format("Game object: {} is not valid, can't destroy it. Was it already destroyed (i.e. by editor)?\n", go_).c_str());
    }
}