#include "base_script.h"
#include "iscene.h"
#include "../app.h"

namespace
{
inline void set_name(engine_scene_t scene, engine_game_object_t go, const char* name)
{
    engine_name_component_t nc{};
    if (!engineSceneHasNameComponent(scene, go))
    {
        engineSceneAddNameComponent(scene, go);
    }
    std::strncpy(nc.name, name, strlen(name));
    engineSceneUpdateNameComponent(scene, go, &nc);
}
} // namespace anonymous

project_c::BaseNode::BaseNode(engine::IScene* my_scene, std::string_view name)
    : BaseNode(my_scene, engineSceneCreateGameObject(my_scene->get_handle()), name)
{
}

project_c::BaseNode::BaseNode(engine::IScene* my_scene, engine_game_object_t go, const AnimationController& anim_c, std::string_view name)
    : engine::IScript(my_scene, go)
    , anim_controller_(anim_c)
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();
    set_name(scene, go_, name.data());
}

project_c::BaseNode::BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name)
    : BaseNode(my_scene, go, {}, name)
{
}

project_c::BaseNode::BaseNode(engine::IScene* my_scene, const PrefabResult& pr, std::string_view name)
    : BaseNode(my_scene, pr.go, pr.anim_controller, name)
{

}

void project_c::BaseNode::set_world_position(float x, float y, float z)
{
    auto tc = engineSceneGetTransformComponent(my_scene_->get_handle(), go_);
    tc.position[0] = x;
    tc.position[1] = y;
    tc.position[2] = z;
    engineSceneUpdateTransformComponent(my_scene_->get_handle(), go_, &tc);
}
