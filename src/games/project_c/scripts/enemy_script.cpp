#include "enemy_script.h"
#include "player_script.h"
#include "enviorment_script.h"
#include "scripts_utils.h"
#include "coin_script.h"

#include "../app.h"
#include "../scenes/scene_test.h"

#include "../nav_mesh.h"

#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

project_c::Enemy::Enemy(engine::IScene* my_scene, const PrefabResult& pr, const NavMesh* nav_mesh, float offset_x, float offset_z)
    : BaseNode(my_scene, pr, "enemy")
    , state_(States::DECISION_MAKE)
    , nav_mesh_(nav_mesh)
{
    auto tc = engineGameObjectGetTransformComponent(go_);

    tc.position[0] = offset_x;
    tc.position[1] += 0.25f;
    tc.position[2] = offset_z;

    tc.scale[0] = 0.35f;
    tc.scale[1] = 0.35f;
    tc.scale[2] = 0.35f;

    engineGameObjectUpdateTransformComponent(go_, &tc);
    collect_hit_flash_materials(go_);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[0] = 0.0f;
        child_c.transform[1] = 1.0f;
        child_c.transform[2] = 0.0f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.7f, 1.0f, 0.5f});
    }
    engineGameObjectUpdateColliderComponent(go_, &cc);

    //rb
    auto rbc = engineGameObjectAddRigidBodyComponent(go_);
    rbc.mass = 100000.0f;
    engineGameObjectUpdateRigidBodyComponent(go_, &rbc);
}

project_c::Enemy::~Enemy()
{
    auto typed_scene = static_cast<project_c::TestScene*>(my_scene_);
    assert(typed_scene);
    typed_scene->ui_remove_enemy(this);
    //assert(engineGameObjectIsValid(go_));
    utils::delete_game_objects_hierarchy(go_);
}

void project_c::Enemy::update(float dt)
{
    engine::ScopedProfiler prof("project_c::Enemy::update");

    auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    if (hit_stop_remaining_seconds_ > 0.0f)
    {
        if (hit_stop_just_started_)
        {
            hit_stop_just_started_ = false;
        }
        else
        {
            hit_stop_remaining_seconds_ = std::max(0.0f, hit_stop_remaining_seconds_ - dt / 1000.0f);
        }

        if (hit_stop_remaining_seconds_ > 0.0f)
        {
            return;
        }
        if (!engineAnimationControllerSetPaused(animation_controller, false))
        {
            engineLog("Failed to resume enemy animation after hit-stop.\n");
        }
    }

    if (hit_flash_remaining_seconds_ > 0.0f)
    {
        hit_flash_remaining_seconds_ = std::max(0.0f, hit_flash_remaining_seconds_ - dt / 1000.0f);
        update_hit_flash(hit_flash_remaining_seconds_ / 0.12f);
    }

    if (player_go_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        player_go_ = utils::get_game_objects_with_name("player")[0];
    }

    for (auto& s : debug_scripts_)
    {
        my_scene_->unregister_script(s);
    }
    debug_scripts_.clear();

    auto tc = engineGameObjectGetTransformComponent(go_);
    auto ec = engineGameObjectGetTransformComponent(player_go_);
    const auto distance_to_player = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(ec.position[0], ec.position[2]));

    const auto my_node_idx =  nav_mesh_->get_node_idx({ tc.position[0], tc.position[1], tc.position[2] });
    const auto player_node_idx =  nav_mesh_->get_node_idx({ ec.position[0], ec.position[1], ec.position[2] });
    auto path = [&]()
        {
            if (distance_to_player < 0.8f || my_node_idx == -1 || player_node_idx == -1)
            {
                return NavMeshPathFinder::PathFromStartToEnd{};
            }
            return NavMeshPathFinder::find_path(*nav_mesh_, my_node_idx, player_node_idx);
        }();

    if (hp <= 0 && state_ != States::DIE)
    {
        state_ = States::DIE;
        engineAnimationControllerAnimationPlay(animation_controller, "Death_A", 0);
        // remove collider so enemy will not be hit by players attacks
        engineGameObjectRemoveColliderComponent(go_);
        if (reinterpret_cast<AppProjectC*>(my_scene_->get_app())->is_prefab_available(project_c::PrefabType::PREFAB_TYPE_COIN_GOLD))
        {
            auto coin = my_scene_->register_script<project_c::Coin>(reinterpret_cast<AppProjectC*>(my_scene_->get_app())->instantiate_prefab(project_c::PREFAB_TYPE_COIN_GOLD, my_scene_).go);
            coin->set_world_position(tc.position[0], tc.position[1] + 1.0f, tc.position[2]);
        }
    }

    switch (state_)
    {
    case States::DECISION_MAKE:
    {
        if (path.nodes.size() == 0 && distance_to_player < 0.8f)
        {
            state_ = States::ATTACK;
            if (!engineAnimationControllerIsAnimationPlaying(animation_controller, attack_data_.get_animation_name()))
            {
                engineAnimationControllerAnimationPlay(animation_controller, attack_data_.get_animation_name(), 0);
            }
        }
        else if (path.nodes.size() >= 1 && path.nodes.size() < 3)
        {
            state_ = States::MOVE;
        }
        else
        {
            state_ = States::IDLE;
        }
        break;
    }
    case States::IDLE:
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Idle"))
        {
            engineAnimationControllerAnimationPlay(animation_controller, "Idle", 0);
        }
        state_ = States::DECISION_MAKE;
        break;
    }
    case States::ATTACK:
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, attack_data_.get_animation_name()))
        {
            state_ = States::DECISION_MAKE;
            attack_data_.attack_with_right = !attack_data_.attack_with_right;
        }

        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        engineGameObjectUpdateTransformComponent(go_, &tc);
        break;
    }
    case States::DIE:
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Death_A"))
        {
            my_scene_->unregister_script(this);
        }
        break;
    }
    case States::MOVE:
    {
        // path is computed per_frame, but decision was made frame before
        // so it can happen that target has moved and new position is not reachable or close to current position
        if (path.nodes.empty())
        {
            state_ = States::DECISION_MAKE;
            break;
        }
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Running_A"))
        {
            engineAnimationControllerAnimationPlay(animation_controller, "Running_A", 0);
        }
        for (auto& node : path.nodes)
        {
            const auto n_pos = nav_mesh_->get_node(node).get_center();
            debug_scripts_.push_back(my_scene_->register_script<project_c::DebugPathNode>(n_pos.x, n_pos.z));
        }
        const auto& target_node = nav_mesh_->get_node(path.nodes[0]);
        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), target_node.get_center());
        //auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        const float speed_cooef = 0.002f;
        const float speed = speed_cooef * dt;
        const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, 1.0f));
        tc.position[0] += forward.x * speed;
        //tc.position[1] += forward.y * speed;
        tc.position[2] += forward.z * speed;
        engineGameObjectUpdateTransformComponent(go_, &tc);
        //state_ = States::DECISION_MAKE;
        break;
    }
    default:
    {
        engineLog("Unknown enemy state\n");
        break;
    }
    }
    auto typed_scene = static_cast<project_c::TestScene*>(my_scene_);
    typed_scene->ui_update_enemy(this);
}

void project_c::Enemy::receive_attack_hit(std::uint32_t damage, float hit_stop_seconds)
{
    hp = damage >= hp ? 0 : hp - damage;
    hit_flash_remaining_seconds_ = 0.12f;
    update_hit_flash(1.0f);

    hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, hit_stop_seconds);
    hit_stop_just_started_ = true;
    const auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    if (!engineAnimationControllerSetPaused(animation_controller, true))
    {
        engineLog("Failed to pause enemy animation for hit-stop.\n");
    }
}

void project_c::Enemy::collect_hit_flash_materials(engine_game_object_t game_object)
{
    if (!engineGameObjectIsValid(game_object))
    {
        return;
    }

    if (engineGameObjectHasMaterialComponent(game_object))
    {
        const auto material = engineGameObjectGetMaterialComponent(game_object);
        if (material.type == ENGINE_MATERIAL_TYPE_PONG)
        {
            HitFlashMaterial entry{};
            entry.game_object = game_object;
            std::copy(
                std::begin(material.data.pong.diffuse_color),
                std::end(material.data.pong.diffuse_color),
                entry.original_diffuse_color.begin());
            hit_flash_materials_.push_back(entry);
        }
    }

    if (engineGameObjectHasChildrenComponent(game_object))
    {
        const auto children = engineGameObjectGetChildrenComponent(game_object);
        for (const auto child : children.child)
        {
            if (child != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                collect_hit_flash_materials(child);
            }
        }
    }
}

void project_c::Enemy::update_hit_flash(float strength)
{
    constexpr std::array<float, 3> hit_color = { 1.0f, 0.32f, 0.22f };
    strength = std::clamp(strength, 0.0f, 1.0f);

    for (const auto& entry : hit_flash_materials_)
    {
        if (!engineGameObjectIsValid(entry.game_object) || !engineGameObjectHasMaterialComponent(entry.game_object))
        {
            continue;
        }

        auto material = engineGameObjectGetMaterialComponent(entry.game_object);
        if (material.type != ENGINE_MATERIAL_TYPE_PONG)
        {
            continue;
        }

        for (std::size_t channel = 0; channel < hit_color.size(); ++channel)
        {
            const float original = entry.original_diffuse_color[channel];
            material.data.pong.diffuse_color[channel] = original + (hit_color[channel] - original) * strength;
        }
        engineGameObjectUpdateMaterialComponent(entry.game_object, &material);
    }
}
