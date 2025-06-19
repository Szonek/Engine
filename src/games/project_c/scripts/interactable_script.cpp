#include "interactable_script.h"

#include <iscene.h>

project_c::Chest::Chest(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "chest")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[1] = 0.2f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.4f, 0.2f, 0.3f});
    }
    engineSceneUpdateColliderComponent(scene, go_, &cc);

}

void project_c::Chest::interact()
{
    // placeholder: change color to dark
    auto scene = my_scene_->get_handle();
    auto mc = engineSceneGetMeshComponent(scene, go_);
    if (mc.geometry != ENGINE_INVALID_OBJECT_HANDLE)
    {
        auto mat = engineSceneGetMaterialComponent(scene, go_);
        set_c_array(mat.data.pong.diffuse_color, std::array<float, 4>{ 0.2f, 0.2f, 0.2f, 1.0f });
        engineSceneUpdateMaterialComponent(scene, go_, &mat);
    }
    else
    {
        engineLog("Chest has no mesh component to change color!\n");
    }
}
