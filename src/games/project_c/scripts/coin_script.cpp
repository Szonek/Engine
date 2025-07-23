#include "coin_script.h"
#include "player_script.h"
#include <iscene.h>

project_c::Coin::Coin(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "coin")
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.scale[0] *= 0.35f;
    tc.scale[1] *= 0.35f;
    tc.scale[2] *= 0.35f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // physcis
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[1] = 0.0f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.35f, 0.35f, 0.15f});
    }
    engineGameObjectUpdateColliderComponent(go_, &cc);

    auto rc = engineGameObjectAddRigidBodyComponent(go_);
    rc.mass = 100.0f; // Set a small mass for the coin
    engineGameObjectUpdateRigidBodyComponent(go_, &rc);
}

void project_c::Coin::update(float dt)
{   
    // Coins are usually not updated, but if needed, you can add logic here.
    // For example, you might want to rotate the coin
}

void project_c::Coin::on_collision(const collision_t& info)
{
    // Handle collision logic for the coin, if necessary.
    // For example, you might want to play a sound or trigger an effect.
    if (auto* player = my_scene_->get_script<project_c::Player>(info.other))
    {
        player->add_coin(1); // Assuming Player has a method to add coins
        my_scene_->unregister_script(this); // Remove the coin from the scene
    }
}

void project_c::Coin::push_force(float x, float y, float z, engine_force_type_t type)
{
    enginePhysicsAddForce(go_, std::array<float, 3>{x, y, z}.data(), type);
}