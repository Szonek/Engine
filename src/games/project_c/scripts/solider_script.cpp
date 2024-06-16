#include "solider_script.h"
#include "scripts_utils.h"
#include "enemy_script.h"

#include "../app.h"
#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

project_c::Sword::Sword(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "weapon-sword")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();
    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[0] = -0.2f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.1f;

    auto rotation = glm::angleAxis(glm::radians(-65.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    std::memcpy(tc.rotation, glm::value_ptr(rotation), sizeof(tc.rotation));
    engineSceneUpdateTransformComponent(scene, go_, &tc);


    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = true;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[1] -= 0.2f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.05f, 0.20f, 0.005f});
    engineSceneUpdateColliderComponent(scene, go_, &cc);

    // parent to hand
    const auto parent = utils::get_game_objects_with_name(scene, "arm-right")[0];
    if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        auto pc = engineSceneAddParentComponent(scene, go_);
        pc.parent = parent;
        engineSceneUpdateParentComponent(scene, go_, &pc);
    }
}


project_c::Dagger::Dagger(engine::IScene* my_scene, engine_game_object_t go, const Config& config)
    : BaseNode(my_scene, go, "dagger")
    , config_(config)
{
    const auto scene = my_scene->get_handle();
    const auto app = my_scene->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go);
    tc.position[0] = config.start_position[0];
    tc.position[1] = config.start_position[1];
    tc.position[2] = config.start_position[2];

    tc.scale[0] = 1.5f;
    tc.scale[1] = 1.5f;
    tc.scale[2] = 1.5f;

    auto rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    rotation *= config.direction;
    std::memcpy(tc.rotation, glm::value_ptr(rotation), sizeof(tc.rotation));
    engineSceneUpdateTransformComponent(scene, go, &tc);

    // collider
    auto cc = engineSceneAddColliderComponent(scene, go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.is_trigger = true;
    cc.collider.box.size[0] = 0.05f;
    cc.collider.box.size[1] = 0.05f;
    cc.collider.box.size[2] = 0.05f;
    engineSceneUpdateColliderComponent(scene, go, &cc);

    // material
    auto mc = engineSceneAddMaterialComponent(scene, go);
    mc.material = engineApplicationGetMaterialByName(app, "dagger_01");
    engineSceneUpdateMaterialComponent(scene, go, &mc);
}


void project_c::Dagger::update(float dt)
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();
    auto tc = engineSceneGetTransformComponent(scene, go_);

    const float speed_cooef = 0.002f;
    const float speed = speed_cooef * dt;
    const glm::vec3 forward = glm::normalize(config_.direction * glm::vec3(0.0f, 0.0f, 1.0f));
    tc.position[0] += forward.x * speed;
    tc.position[2] += forward.z * speed;
    engineSceneUpdateTransformComponent(scene, go_, &tc);
}


project_c::AttackTrigger::AttackTrigger(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "attack-trigger")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    // transform
    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = true;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[1] = 0.21f;
    cc_child.transform[2] = 0.6f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.3f, 0.05f, 0.3f});
    engineSceneUpdateColliderComponent(scene, go_, &cc);

    // parent to root
    const auto gos_with_root_name = utils::get_game_objects_with_name(scene, "solider");
    for (auto& parent : gos_with_root_name)
    {
        if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            auto pc = engineSceneAddParentComponent(scene, go_);
            pc.parent = parent;
            engineSceneUpdateParentComponent(scene, go_, &pc);
            break;
        }
    }
}

void project_c::AttackTrigger::on_collision(const collision_t& info)
{
    if (is_active_)
    {
        if (auto* enemy = my_scene_->get_script<Enemy>(info.other))
        {
            enemy->hp -= 10;
        }
    }
}

void project_c::AttackTrigger::update(float dt)
{
    // deactivate trigger after one frame?
    if (is_active_)
    {
        is_active_ = false;
    }
}

void project_c::AttackTrigger::activate()
{
    is_active_ = true;
}

project_c::Solider::Solider(engine::IScene* my_scene, const PrefabResult& pr)
    : BaseNode(my_scene, pr, "solider")
    , attack_trigger_(nullptr)
    , state_(States::IDLE)
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[1] = -0.25f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = false;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[1] = 0.45f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.3f, 0.35f, 0.2f});
    engineSceneUpdateColliderComponent(scene, go_, &cc);

    //rb
    auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
    rbc.mass = 100000.0f;
    engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);

    // add attack trigger
    attack_trigger_ = my_scene_->register_script<AttackTrigger>(engineSceneCreateGameObject(my_scene->get_handle()));
    auto my_app = dynamic_cast<project_c::AppProjectC*>(my_scene_->get_app());
    assert(my_app != nullptr);
    my_scene_->register_script<project_c::Sword>(my_app->instantiate_prefab(project_c::PREFAB_TYPE_SWORD, my_scene).go);
}

void project_c::Solider::update(float dt)
{
    anim_controller_.update(dt);
    dodge_data_.update(dt);
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();


    auto rotate_towards_global_target = [&]()
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(global_data_.last_mouse_hit.position[0], global_data_.last_mouse_hit.position[1], global_data_.last_mouse_hit.position[2]));
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        };

    const auto lmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT);
    const auto rmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_RIGHT);
    if ((lmb || rmb) && state_ != States::DODGE)
    {
        const auto ray = utils::get_ray_from_mouse_position(app, scene, utils::get_active_camera_game_objects(scene)[0]);
        const std::array<engine_game_object_t, 1> raycast_ignore_list = { attack_trigger_->get_game_object() };
        const auto hit_info = engineScenePhysicsRayCast(scene, raycast_ignore_list.data(), raycast_ignore_list.size(), &ray, 1000.0f);
        if (hit_info.go != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            global_data_.last_mouse_hit = hit_info;
            state_ = lmb ? States::MOVE : States::ATTACK;
        }
    }

    if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE) && dodge_data_.can_dodge())
    {
        state_ = States::DODGE;
        dodge_data_.activate();
        const auto ray = utils::get_ray_from_mouse_position(app, scene, utils::get_active_camera_game_objects(scene)[0]);
    }

    if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_1))
    {
        auto cc = engineSceneGetColliderComponent(scene, attack_trigger_->get_game_object());
        cc.collider.compound.children->collider.box.size[0] = 2.6f;
        cc.collider.compound.children->collider.box.size[2] = 2.6f;
        engineSceneUpdateColliderComponent(scene, attack_trigger_->get_game_object(), &cc);

    }
    else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_2))
    {
        auto cc = engineSceneGetColliderComponent(scene, attack_trigger_->get_game_object());
        cc.collider.compound.children->collider.box.size[0] = 0.3f;
        cc.collider.compound.children->collider.box.size[2] = 0.3f;
        engineSceneUpdateColliderComponent(scene, attack_trigger_->get_game_object(), &cc);
    }
    else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_Q))
    {
        state_ = States::SKILL_1;
    }
    switch (state_)
    {
    case States::IDLE:
    {
        anim_controller_.set_active_animation("idle");
        break;
    }
    case States::DODGE:
    {
        if (!dodge_data_.animation_is_playing())
        {
            state_ = States::IDLE;
        }
        else
        {
            anim_controller_.set_active_animation("crouch");
            const float speed_cooef = 0.015f;
            const float speed = speed_cooef * dt;
            auto tc = engineSceneGetTransformComponent(scene, go_);
            // move
            const glm::quat rotation = glm::make_quat(tc.rotation); // Convert the rotation to a glm::quat
            const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f); // Get the forward direction vector
            const glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)); // Calculate the right direction vector
            tc.position[0] += forward.x * speed;
            //tc.position[1] += forward.y * speed;  // dont go up!
            tc.position[2] += forward.z * speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        break;
    }
    case States::SKILL_1:
    {
        if (skill_1_data_.animation_started)
        {
            if (!anim_controller_.is_active_animation(skill_1_data_.get_animation_name()))
            {
                state_ = States::IDLE;
                skill_1_data_ = {};
            }
        }
        else
        {
            rotate_towards_global_target();
            anim_controller_.set_active_animation(skill_1_data_.get_animation_name());
            skill_1_data_.animation_started = true;
            auto my_app = dynamic_cast<project_c::AppProjectC*>(my_scene_->get_app());

            auto tc = engineSceneGetTransformComponent(scene, go_);
            Dagger::Config config{};
            config.start_position = { tc.position[0], 0.5f, tc.position[2] };
            config.direction = glm::make_quat(tc.rotation);
            auto skill_1 = my_scene_->register_script<project_c::Dagger>(my_app->instantiate_prefab(project_c::PREFAB_TYPE_DAGGER, my_scene_).go, config);
        }
        break;
    }
    case States::ATTACK:
    {
        if (attack_data_.animation_started)
        {
            if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
            {
                state_ = States::IDLE;
                attack_data_ = {};
            }
        }
        else
        {
            rotate_towards_global_target();
            anim_controller_.set_active_animation(attack_data_.get_animation_name());
            attack_data_.animation_started = true;
            attack_trigger_->activate();
        }

        break;
    }
    case States::MOVE:
    {
        const float speed_cooef = 0.0025f;
        const float speed = speed_cooef * dt;

        auto tc = engineSceneGetTransformComponent(scene, go_);
        const auto distance = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(global_data_.last_mouse_hit.position[0], global_data_.last_mouse_hit.position[2]));
        if (distance < 0.05f)
        {
            state_ = States::IDLE;
        }
        else
        {
            anim_controller_.set_active_animation("walk");

            rotate_towards_global_target();
            auto tc = engineSceneGetTransformComponent(scene, go_);
            const float speed_cooef = 0.0025f;
            const float speed = speed_cooef * dt;
            // move
            const glm::quat rotation = glm::make_quat(tc.rotation); // Convert the rotation to a glm::quat
            const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f); // Get the forward direction vector
            const glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)); // Calculate the right direction vector
            tc.position[0] += forward.x * speed;
            //tc.position[1] += forward.y * speed;  // dont go up!
            tc.position[2] += forward.z * speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }

        break;
    }
    default:
    {
        engineLog("Unknown solider state\n");
        break;
    }
    }
}
