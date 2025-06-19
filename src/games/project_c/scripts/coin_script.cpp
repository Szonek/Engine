#include "coin_script.h"
#include <iscene.h>

project_c::Coin::Coin(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "coin")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.scale[0] *= 0.35f;
    tc.scale[1] *= 0.35f;
    tc.scale[2] *= 0.35f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[1] = 0.0f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.35f, 0.35f, 0.15f});
    }
    engineSceneUpdateColliderComponent(scene, go_, &cc);

    auto rc = engineSceneAddRigidBodyComponent(scene, go_);
    rc.mass = 100.0f; // Set a small mass for the coin
    engineSceneUpdateRigidBodyComponent(scene, go_, &rc);
}

void project_c::Coin::update(float dt)
{   
    // Coins are usually not updated, but if needed, you can add logic here.
    // For example, you might want to rotate the coin
}

void project_c::Coin::on_collision(const collision_t& info)
{
}
void project_c::Coin::push_force(float x, float y, float z, engine_force_type_t type)
{
    const auto scene = my_scene_->get_handle();
    engineScenePhysicsAddForce(scene, go_, std::array<float, 3>{x, y, z}.data(), type);
}