#include "iscript.h"
#include "iscene.h"



engine::IScript::IScript(engine::IScene *my_scene)
    : my_scene_(my_scene)
    , go_(engineSceneCreateGameObject(my_scene_->get_handle()))
{}

engine::IScript::~IScript()
{
    engineSceneDestroyGameObject(my_scene_->get_handle(), go_);
}

engine::SceneManager *engine::IScript::get_scene_manager()
{
    assert(my_scene_);
    return my_scene_->get_scene_manager();
}
