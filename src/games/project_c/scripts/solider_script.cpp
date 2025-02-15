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
    set_c_array(mc.data.pong.diffuse_color, std::array<float, 4>{0.9f, 0.9f, 0.9f, 1.0f});
    engineSceneUpdateMaterialComponent(scene, go, &mc);
}


void project_c::Dagger::update(float dt)
{
    if (config_.destroy_on_next_frame)
    {
        my_scene_->unregister_script(this);
        return;
    }
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();
    auto tc = engineSceneGetTransformComponent(scene, go_);

    const float speed_cooef = 0.008f;
    const float speed = speed_cooef * dt;
    const glm::vec3 forward = glm::normalize(config_.direction * glm::vec3(0.0f, 0.0f, 1.0f));
    tc.position[0] += forward.x * speed;
    tc.position[2] += forward.z * speed;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    const auto distance = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(config_.start_position[0], config_.start_position[2]));
    if(distance > 3.5f)
    {
        config_.destroy_on_next_frame = true;
    }
}

void project_c::Dagger::on_collision(const collision_t& info)
{
    if (info.other == config_.ignore_go)
    {
        return;
    }
    if (auto* enemy = my_scene_->get_script<Enemy>(info.other))
    {
        enemy->hp -= 10;
        config_.destroy_on_next_frame = true;
        // spawn next dagger
        if (config_.ricochet_count > 1)
        {
            const auto enemies = utils::get_game_objects_with_name(my_scene_->get_handle(), "enemy");
            if (enemies.size() > 1)
            {
                config_.ricochet_count--;
                engine_game_object_t go_closest = ENGINE_INVALID_GAME_OBJECT_ID;
                float distance = std::numeric_limits<float>::max();
                for (const auto& go : enemies)
                {
                    if (go != info.other)
                    {
                        if (go_closest == ENGINE_INVALID_GAME_OBJECT_ID)
                        {
                            go_closest = go;
                        }
                        else
                        {
                            const auto tc = engineSceneGetTransformComponent(my_scene_->get_handle(), go);
                            const auto tc_closest = engineSceneGetTransformComponent(my_scene_->get_handle(), go_closest);
                            const auto distance_closest = glm::distance(glm::vec2(tc_closest.position[0], tc_closest.position[2]), glm::vec2(tc.position[0], tc.position[2]));
                            if (distance_closest < distance)
                            {
                                distance = distance_closest;
                                go_closest = go;
                            }
                        }
                    }
                }

                if (go_closest != ENGINE_INVALID_GAME_OBJECT_ID)
                {
                    auto my_app = dynamic_cast<project_c::AppProjectC*>(my_scene_->get_app());

                    const auto etc = engineSceneGetTransformComponent(my_scene_->get_handle(), info.other);
                    const auto gctc = engineSceneGetTransformComponent(my_scene_->get_handle(), go_closest);
                    Config ricochet_config{};
                    ricochet_config.ricochet_count = config_.ricochet_count;
                    ricochet_config.start_position = { info.contact_points[0].point[0], info.contact_points[0].point[1], info.contact_points[0].point[2] };

                    ricochet_config.direction = utils::rotate_toward(glm::vec3(etc.position[0], etc.position[1], etc.position[2]), glm::vec3(gctc.position[0], gctc.position[1], gctc.position[2]));
                    ricochet_config.ignore_go = info.other;
                    auto new_dagger = my_scene_->register_script<project_c::Dagger>(my_app->instantiate_prefab(project_c::PREFAB_TYPE_DAGGER, my_scene_).go, ricochet_config);
                }

            }

        }

        return; // to not hit more enemies;
    }
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
    cc_child.transform[1] = 0.35f;
    cc_child.rotation_quaternion[3] = 1.0f;
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
    auto check_state_bit = [&](States state)
        {
            return (state_ & state) != 0;
        };
    auto clear_state_bit = [&](States state)
        {
            state_ &= ~state;
        };
    auto enable_state_bit = [&](States state)
        {
            state_ |= state;
        };

    anim_controller_.update(dt);
    dodge_data_.update(dt);
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    const std::array<engine_game_object_t, 1> raycast_ignore_list = { attack_trigger_->get_game_object() };
    const auto ray = utils::get_ray_from_mouse_position(app, scene, utils::get_active_camera_game_objects(scene)[0]);
    const auto hit_info = engineScenePhysicsRayCast(scene, raycast_ignore_list.data(), raycast_ignore_list.size(), &ray, 1000.0f);

    auto rotate_towards_global_target = [&]()
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            //auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(global_data_.last_mouse_hit.position[0], global_data_.last_mouse_hit.position[1], global_data_.last_mouse_hit.position[2]));
            auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(hit_info.position[0], hit_info.position[1], hit_info.position[2]));
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        };
    rotate_towards_global_target();// rotate towards target

    const auto lmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT);
    const auto rmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_RIGHT);
    if (rmb)
    {
        enable_state_bit(States::ATTACK);
    }

    const auto button_A = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A);
    const auto button_W = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W);
    const auto button_D = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D);
    const auto button_S = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S);
    if (button_A || button_W || button_D || button_S)
    {
        enable_state_bit(States::MOVE);
    }

    if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE) && dodge_data_.can_dodge())
    {
        enable_state_bit(States::DODGE);
        dodge_data_.activate();
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
        enable_state_bit(States::SKILL_1);
    }

    if (state_ == States::IDLE)
    {
        anim_controller_.set_active_animation("idle");
    }
    if (check_state_bit(States::DODGE))
    {
        if (!dodge_data_.animation_is_playing())
        {
            clear_state_bit(States::DODGE);
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
    }
    if (check_state_bit(States::MOVE))
    {
        anim_controller_.set_active_animation(move_data_.get_animation_name());
        move_data_.animation_started = true;

        auto tc = engineSceneGetTransformComponent(scene, go_);
        const float speed_cooef = 0.0025f;
        const float speed = [&]()
            {
                auto ret = speed_cooef * dt;
                //const auto move_buttons_pressed_count = static_cast<float>(button_W + button_S + button_A + button_D);
                //if (move_buttons_pressed_count)
                //{
                //    ret /= move_buttons_pressed_count;
                //}
                return ret;
            }();
        if (button_W)  // up
        {
            tc.position[2] -= speed;
        }
        if (button_S) // down
        {
            tc.position[2] += speed;
        }
        if (button_A) // left
        {
            tc.position[0] -= speed;
        }
        if (button_D) // right
        {
            tc.position[0] += speed;
        }

        engineSceneUpdateTransformComponent(scene, go_, &tc);
        clear_state_bit(States::MOVE);
    }
    if (check_state_bit(States::ATTACK))
    {
        /*
        ToDo: attack is bugged due to lack of possiblity to play multiple animations
        // i.e. attack -> (hit enemy), press move button (it will remove attack animation) -> we can attack instantly again (beacuse attack animation was removed due to move animation)
        */
        if (attack_data_.animation_started)
        {
            if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
            {
                clear_state_bit(States::ATTACK);
                attack_data_ = {};
            }
        }
        else if (hit_info.go != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            anim_controller_.set_active_animation(attack_data_.get_animation_name());
            attack_data_.animation_started = true;
            attack_trigger_->activate();
        }
    }
    if (check_state_bit(States::SKILL_1))
    {
        /*
        ToDo: the same bug as with attack animation
        */
        if (skill_1_data_.animation_started)
        {
            if (!anim_controller_.is_active_animation(skill_1_data_.get_animation_name()))
            {
                skill_1_data_ = {};
            }
        }
        else
        {
            //rotate_towards_global_target();
            anim_controller_.set_active_animation(skill_1_data_.get_animation_name());
            skill_1_data_.animation_started = true;
            auto my_app = dynamic_cast<project_c::AppProjectC*>(my_scene_->get_app());

            auto tc = engineSceneGetTransformComponent(scene, go_);
            Dagger::Config config{};
            config.start_position = { tc.position[0], 0.5f, tc.position[2] };
            config.direction = glm::make_quat(tc.rotation);
            config.ricochet_count = 4;
            auto skill_1 = my_scene_->register_script<project_c::Dagger>(my_app->instantiate_prefab(project_c::PREFAB_TYPE_DAGGER, my_scene_).go, config);
        }
        clear_state_bit(States::SKILL_1);
    }
}
