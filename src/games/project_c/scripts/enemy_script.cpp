#include "enemy_script.h"
#include "enviorment_script.h"
#include "scripts_utils.h"

#include "../app.h"

#include "../nav_mesh.h"

#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>


project_c::Enemy::Enemy(engine::IScene* my_scene, const PrefabResult& pr, const NavMesh* nav_mesh, float offset_x, float offset_z)
    : BaseNode(my_scene, pr, "enemy")
    , state_(States::DECISION_MAKE)
    , nav_mesh_(nav_mesh)
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);

    tc.position[0] = offset_x;
    tc.position[1] -= 0.25f;
    tc.position[2] = offset_z;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[0] = 0.0f;
        child_c.transform[1] = 0.35f;
        child_c.transform[2] = 0.0f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.3f, 0.35f, 0.3f});
    }
    engineSceneUpdateColliderComponent(scene, go_, &cc);

    //rb
    auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
    rbc.mass = 1.0f;
    //rbc.mass = 0.0f;
    engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);

    // add health bar
    my_scene_->register_script<project_c::EnemyHealthBar>(this);
}

project_c::Enemy::~Enemy()
{
    my_scene_->unregister_script(health_bar_script_);
    utils::delete_game_objects_hierarchy(my_scene_->get_handle(), go_);
}

void project_c::Enemy::update(float dt)
{
    for (auto& s : debug_scripts_)
    {
        my_scene_->unregister_script(s);
    }
    debug_scripts_.clear();
    anim_controller_.update(dt);
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    const auto player = utils::get_game_objects_with_name(scene, "solider")[0];
    auto tc = engineSceneGetTransformComponent(scene, go_);
    auto ec = engineSceneGetTransformComponent(scene, player);
    const auto distance_to_player = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(ec.position[0], ec.position[2]));

    const auto my_node_idx = -1;// nav_mesh_->get_node_idx({ tc.position[0], tc.position[1], tc.position[2] });
    const auto player_node_idx = -1;// nav_mesh_->get_node_idx({ ec.position[0], ec.position[1], ec.position[2] });
    auto path = [&]()
        {
            if (distance_to_player < 0.8f || my_node_idx == -1 || player_node_idx == -1)
            {
                return NavMeshPathFinder::PathFromStartToEnd{};
            }
            return NavMeshPathFinder::find_path(*nav_mesh_, my_node_idx, player_node_idx);
        }();

    switch (state_)
    {
    case States::DECISION_MAKE:
    {
        if (hp <= 0)
        {
            state_ = States::DIE;
            anim_controller_.set_active_animation("die");
            // remove collider so enemy will not be hit by players attacks
            engineSceneRemoveColliderComponent(scene, go_);
        }
        else
        {
            if (path.nodes.size() == 0 && distance_to_player < 0.8f)
            {
                state_ = States::ATTACK;
                anim_controller_.set_active_animation(attack_data_.get_animation_name());
            }
            else if (path.nodes.size() >= 1 && path.nodes.size() < 6)
            {
                state_ = States::MOVE;
            }
            else
            {
                state_ = States::IDLE;
            }
        }
        break;
    }
    case States::IDLE:
    {
        anim_controller_.set_active_animation("idle");
        state_ = States::DECISION_MAKE;
        break;
    }
    case States::ATTACK:
    {
        if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
        {
            state_ = States::DECISION_MAKE;
            attack_data_.attack_with_right = !attack_data_.attack_with_right;
        }

        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        engineSceneUpdateTransformComponent(scene, go_, &tc);
        break;
    }
    case States::DIE:
    {
        if (!anim_controller_.is_active_animation("die"))
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
        anim_controller_.set_active_animation("walk");
        for (auto& node : path.nodes)
        {
            const auto n_pos = nav_mesh_->get_node(node).get_center();
            debug_scripts_.push_back(my_scene_->register_script<project_c::DebugPathNode>(n_pos.x, n_pos.z));
        }
        const auto target_node = nav_mesh_->get_node(path.nodes[0]);
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
        engineSceneUpdateTransformComponent(scene, go_, &tc);
        state_ = States::DECISION_MAKE;
        break;
    }
    default:
    {
        engineLog("Unknown enemy state\n");
        break;
    }
    }
}

project_c::EnemyHealthBar::EnemyHealthBar(engine::IScene* my_scene, const Enemy* enemy)
    : BaseNode(my_scene, "enemy_health_bar")
    , enemy_(enemy)
    , max_hp_(enemy->hp)
{
    const auto scene = my_scene_->get_handle();

    auto pc = engineSceneAddParentComponent(scene, go_);
    pc.parent = enemy->get_game_object();
    engineSceneUpdateParentComponent(scene, go_, &pc);

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[1] += 1.0f;
    tc.scale[0] = 1.0f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.02f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    auto sc = engineSceneAddSpriteComponent(scene, go_);
    //sc.width = 1.0f;
    //sc.height = 1.0f;
    engineSceneUpdateSpriteComponent(scene, go_, &sc);   
}

project_c::EnemyHealthBar::~EnemyHealthBar()
{
}

void project_c::EnemyHealthBar::update(float dt)
{
    //assert(enemy_);
    const auto enemy_pos = enemy_->hp;

}
