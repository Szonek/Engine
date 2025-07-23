#include "base_script.h"
#include "iscene.h"
#include "../app.h"

namespace
{
inline void set_name(engine_game_object_t go, const char* name)
{
    engine_name_component_t nc{};
    if (!engineHasNameComponent(go))
    {
        engineAddNameComponent(go);
    }
    std::strncpy(nc.name, name, strlen(name));
    engineUpdateNameComponent(go, &nc);
}
} // namespace anonymous


project_c::BaseNode::BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name)
    : engine::IScript(my_scene, go)
{
    set_name(go_, name.data());
}

project_c::BaseNode::BaseNode(engine::IScene* my_scene, std::string_view name)
    : BaseNode(my_scene, engineCreateGameObject(), name)
{
}

project_c::BaseNode::BaseNode(engine::IScene* my_scene, const PrefabResult& pr, std::string_view name)
    : BaseNode(my_scene, pr.go, name)
{

}

void project_c::BaseNode::set_world_position(float x, float y, float z)
{
    auto tc = engineGetTransformComponent(go_);
    tc.position[0] = x;
    tc.position[1] = y;
    tc.position[2] = z;
    engineUpdateTransformComponent(go_, &tc);
}
