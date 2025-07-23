#include "interactable_script.h"
#include <iscene.h>
#include "coin_script.h"
#include "../app.h"
#include "../prefab_types.h"
#include <cassert>

project_c::Chest::Chest(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "chest")
{
    // physcis
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[1] = 0.2f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.4f, 0.2f, 0.3f});
    }
    engineGameObjectUpdateColliderComponent(go_, &cc);

}

void project_c::Chest::interact()
{
    if (was_interacted_)
    {
        engineLog("Chest was already interacted with!\n");
        return;
    }
    // placeholder: change color to dark
    auto mc = engineGameObjectGetMeshComponent(go_);
    if (mc.geometry != ENGINE_INVALID_OBJECT_HANDLE)
    {
        // drop coin, ToDO: random items?
        const auto tc = engineGameObjectGetTransformComponent(go_);
        auto coin = my_scene_->register_script<project_c::Coin>(reinterpret_cast<AppProjectC*>(my_scene_->get_app())->instantiate_prefab(project_c::PREFAB_TYPE_COIN_GOLD, my_scene_).go);
        coin->set_world_position(tc.position[0], tc.position[1] + 1.0f, tc.position[2]);
        coin->push_force(0.0f, 20.0f, 150.0f, ENGINE_FORCE_TYPE_IMPLUSE);
        // update chest to dark color to simulate opened chest
        auto mat = engineGameObjectGetMaterialComponent(go_);
        set_c_array(mat.data.pong.diffuse_color, std::array<float, 4>{ 0.3f, 0.3f, 0.3f, 1.0f });
        engineGameObjectUpdateMaterialComponent(go_, &mat);
        was_interacted_ = true;
    }
    else
    {
        engineLog("Chest has no mesh component to change color!\n");
        assert(false);
    }
}
