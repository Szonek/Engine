#include "base_script.h"
#include "iscene.h"
#include "scripts_utils.h"
#include "../app.h"



project_c::BaseNode::BaseNode(engine::IScene* my_scene, engine_game_object_t go)
    : engine::IScript(my_scene, go)
{
}

project_c::BaseNode::BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name)
    : engine::IScript(my_scene, go)
{
    utils::set_name(go_, name);
}

void project_c::BaseNode::set_world_position(float x, float y, float z)
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = x;
    tc.position[1] = y;
    tc.position[2] = z;
    engineGameObjectUpdateTransformComponent(go_, &tc);
}
