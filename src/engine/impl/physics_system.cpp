#include "physics_system.h"
#include "logger.h"
#include "math_helpers.h"
#include "profiler.h"
#include "components_utils/components_initializers.h"
#include "components_internals/physics_component.h"
#include "components_internals/character_controller_internal_component.h"

#include <fmt/format.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>

namespace
{
// the body of a collider or character controller (a game object should have one of them)
void add_physics_internal_component(entt::registry& registry, entt::entity entity)
{
    static_cast<void>(registry.get_or_emplace<engine::physcic_internal_component_t>(entity));
}

void remove_physics_internal_component_of_collider(entt::registry& registry, entt::entity entity)
{
    if (!registry.any_of<engine_character_controller_component_t>(entity))
    {
        registry.remove<engine::physcic_internal_component_t>(entity);
    }
}

void remove_physics_internal_component_of_character_controller(entt::registry& registry, entt::entity entity)
{
    if (!registry.any_of<engine_collider_component_t>(entity))
    {
        registry.remove<engine::physcic_internal_component_t>(entity);
    }
}
}  // namespace

engine::PhysicsSystem::PhysicsSystem(entt::registry& registry, JointMatrixFunction joint_matrix, std::function<bool()> is_editing)
    : registry_(registry)
    , joint_matrix_(std::move(joint_matrix))
    , is_editing_(std::move(is_editing))
    , moved_bodies_(registry.storage<entt::reactive>(entt::hashed_string{"engine::moved_bodies"}))
{
    moved_bodies_.on_update<engine_transform_component_t>();
    // Bodies are created as soon as their components exist (like Unity and Unreal) and created again when they change:
    // physics queries during the scripts' update see game objects created earlier in the frame.
    // EnTT calls listeners last-connected first, so these run after the component setup connected below (and after the
    // transform's initializer, which the scene connects after constructing this system).
    registry_.on_construct<engine_transform_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_construct<engine_collider_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_update<engine_collider_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_construct<engine_rigid_body_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_update<engine_rigid_body_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_destroy<engine_rigid_body_component_t>().connect<&PhysicsSystem::on_rigid_body_removed>(*this);
    registry_.on_construct<engine_character_controller_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_update<engine_character_controller_component_t>().connect<&PhysicsSystem::on_body_component_changed>(*this);
    registry_.on_destroy<engine_character_controller_component_t>().connect<&PhysicsSystem::on_character_controller_removed>(*this);
    // bodies follow their transforms lazily: before the physics step and before physics queries, when anything moved
    registry_.on_update<engine_transform_component_t>().connect<&PhysicsSystem::on_transforms_changed>(*this);
    registry_.on_construct<engine_parent_component_t>().connect<&PhysicsSystem::on_transforms_changed>(*this);
    registry_.on_update<engine_parent_component_t>().connect<&PhysicsSystem::on_transforms_changed>(*this);
    registry_.on_destroy<engine_parent_component_t>().connect<&PhysicsSystem::on_transforms_changed>(*this);
    registry_.on_update<engine_joint_attachment_component_t>().connect<&PhysicsSystem::on_transforms_changed>(*this);
    registry_.on_destroy<engine_joint_attachment_component_t>().connect<&PhysicsSystem::on_transforms_changed>(*this);

    // component setup
    registry_.on_construct<engine_rigid_body_component_t>().connect<&initialize_rigidbody_component>();
    registry_.on_construct<engine_collider_component_t>().connect<&initialize_collider_component>();
    registry_.on_construct<engine_collider_component_t>().connect<&add_physics_internal_component>();
    registry_.on_destroy<engine_collider_component_t>().connect<&remove_physics_internal_component_of_collider>();
    registry_.on_construct<engine_character_controller_component_t>().connect<&initialize_character_controller_component>();
    registry_.on_construct<engine_character_controller_component_t>().connect<&add_physics_internal_component>();
    registry_.on_construct<engine_character_controller_component_t>().connect<&entt::registry::emplace<character_controller_internal_component_t>>();
    registry_.on_destroy<engine_character_controller_component_t>().connect<&remove_physics_internal_component_of_character_controller>();
    registry_.on_destroy<engine_character_controller_component_t>().connect<&entt::registry::remove<character_controller_internal_component_t>>();
    registry_.on_destroy<physcic_internal_component_t>().connect<&PhysicsWorld::remove_rigid_body>(&world_);
}

engine::PhysicsSystem::~PhysicsSystem()
{
    // the listeners bound to this system and its world (the free functions don't reference it)
    registry_.on_construct<engine_transform_component_t>().disconnect(this);
    registry_.on_update<engine_transform_component_t>().disconnect(this);
    registry_.on_construct<engine_collider_component_t>().disconnect(this);
    registry_.on_update<engine_collider_component_t>().disconnect(this);
    registry_.on_construct<engine_rigid_body_component_t>().disconnect(this);
    registry_.on_update<engine_rigid_body_component_t>().disconnect(this);
    registry_.on_destroy<engine_rigid_body_component_t>().disconnect(this);
    registry_.on_construct<engine_character_controller_component_t>().disconnect(this);
    registry_.on_update<engine_character_controller_component_t>().disconnect(this);
    registry_.on_destroy<engine_character_controller_component_t>().disconnect(this);
    registry_.on_construct<engine_parent_component_t>().disconnect(this);
    registry_.on_update<engine_parent_component_t>().disconnect(this);
    registry_.on_destroy<engine_parent_component_t>().disconnect(this);
    registry_.on_update<engine_joint_attachment_component_t>().disconnect(this);
    registry_.on_destroy<engine_joint_attachment_component_t>().disconnect(this);
    registry_.on_destroy<physcic_internal_component_t>().disconnect(&world_);
}

void engine::PhysicsSystem::update(float dt)
{
    ENGINE_PROFILE_SECTION_N("physics_update");
    // also when no transform changed: animations move what is attached to joints
    sync_bodies_to_transforms(true);
    bodies_created_this_frame_.clear();
    apply_forces();
    world_.update(dt / 1000.0f);
    write_dynamic_bodies_to_transforms();
}

engine_result_code_t engine::PhysicsSystem::move_character(entt::entity entity, const glm::vec3& velocity, float dt, engine_character_controller_move_result_t& out_result)
{
    const auto* character = registry_.try_get<engine_character_controller_component_t>(entity);
    if (!character || !registry_.all_of<engine_transform_component_t>(entity))
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    if (follows_other_game_objects(entity))
    {
        log::log(log::LogLevel::eError, fmt::format("[Entity: {}] A character controller can't move a game object with a parent or joint attachment.\n", static_cast<std::uint32_t>(entity)));
        return ENGINE_RESULT_CODE_FAIL;
    }
    // what scripts moved earlier this frame blocks it where it is now
    sync_bodies_to_transforms(false);
    // the transform is the world transform (no parent); this frame's, the world transform component is from the last update
    const auto& transform = registry_.get<engine_transform_component_t>(entity);
    const auto local_to_world = compute_local_matrix(transform);
    auto& state = registry_.get<character_controller_internal_component_t>(entity);
    const float dt_seconds = dt / 1000.0f;
    state.vertical_speed += world_.get_gravity().y * dt_seconds;
    const auto motion = velocity * dt_seconds + glm::vec3(0.0f, state.vertical_speed * dt_seconds, 0.0f);
    const float step_height = character->step_height * glm::length(glm::vec3(local_to_world[1]));
    const auto result = world_.move_character(registry_.get<physcic_internal_component_t>(entity).rigid_body, glm::make_vec3(transform.position),
        motion, step_height, character->max_slope);
    if ((result.blocked_below && state.vertical_speed < 0.0f) || (result.hit_ceiling && state.vertical_speed > 0.0f))
    {
        state.vertical_speed = 0.0f;
    }
    auto updated = transform;
    std::memcpy(updated.position, glm::value_ptr(result.position), sizeof(updated.position));
    registry_.replace<engine_transform_component_t>(entity, updated);
    out_result.grounded = result.grounded;
    out_result.hit_wall = result.hit_wall;
    out_result.hit_ceiling = result.hit_ceiling;
    return ENGINE_RESULT_CODE_OK;
}

bool engine::PhysicsSystem::raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, std::span<const engine_game_object_t> ignore_list, bool hit_triggers, engine_ray_hit_t& out_hit)
{
    ENGINE_PROFILE_SECTION;
    // what scripts moved earlier this frame is hit where it is now
    sync_bodies_to_transforms(false);
    return world_.raycast(origin, direction, max_distance, ignore_list, hit_triggers, out_hit);
}

bool engine::PhysicsSystem::add_force(entt::entity entity, std::array<float, 3> force, engine_force_type_t type)
{
    ENGINE_PROFILE_SECTION;
    if (!registry_.all_of<physcic_internal_component_t>(entity))
    {
        return false;
    }
    if (type != ENGINE_FORCE_TYPE_FORCE && type != ENGINE_FORCE_TYPE_IMPLUSE)
    {
        log::log(log::LogLevel::eError, fmt::format("Unknown force type: {}\n", static_cast<std::uint32_t>(type)));
        return false;
    }
    forces_to_apply_.push_back({ entity, force, type });
    return true;
}

void engine::PhysicsSystem::set_gravity(std::array<float, 3> gravity)
{
    ENGINE_PROFILE_SECTION;
    world_.set_gravity(gravity);
}

const std::vector<engine::CollisionDesc>& engine::PhysicsSystem::get_collisions() const
{
    ENGINE_PROFILE_SECTION;
    return world_.get_collisions();
}

void engine::PhysicsSystem::enable_debug_draw(bool enable)
{
    world_.enable_debug_draw(enable);
}

bool engine::PhysicsSystem::is_debug_draw_enabled() const
{
    return world_.is_debug_drawer_enabled();
}

void engine::PhysicsSystem::collect_debug_lines(std::vector<DebugLineGpuData>& out_lines)
{
    world_.collect_debug_lines(out_lines);
}

bool engine::PhysicsSystem::follows_other_game_objects(entt::entity entity) const
{
    return registry_.any_of<engine_parent_component_t, engine_joint_attachment_component_t>(entity);
}

engine::PhysicsWorld::BodyType engine::PhysicsSystem::get_body_type(entt::entity entity) const
{
    if (registry_.all_of<engine_character_controller_component_t>(entity))
    {
        return PhysicsWorld::BodyType::eKinematic;
    }
    if (const auto* rigid_body = registry_.try_get<engine_rigid_body_component_t>(entity))
    {
        switch (rigid_body->type)
        {
        case ENGINE_RIGID_BODY_TYPE_KINEMATIC:
            return PhysicsWorld::BodyType::eKinematic;
        case ENGINE_RIGID_BODY_TYPE_STATIC:
            return PhysicsWorld::BodyType::eStatic;
        default:
            return rigid_body->mass > 0.0f ? PhysicsWorld::BodyType::eDynamic : PhysicsWorld::BodyType::eStatic;
        }
    }
    return get_collider_body_type(entity);
}

engine::PhysicsWorld::BodyType engine::PhysicsSystem::get_collider_body_type(entt::entity entity) const
{
    // a collider alone moves with its parent or joint, if it has one
    return follows_other_game_objects(entity) ? PhysicsWorld::BodyType::eKinematic : PhysicsWorld::BodyType::eStatic;
}

glm::mat4 engine::PhysicsSystem::get_world_matrix(entt::entity entity) const
{
    return compute_world_matrix(registry_, entity, joint_matrix_);
}

void engine::PhysicsSystem::create_body(entt::entity entity, bool without_rigid_body, bool without_character_controller)
{
    world_.remove_rigid_body(registry_, entity);
    const auto* character = without_character_controller ? nullptr : registry_.try_get<engine_character_controller_component_t>(entity);
    auto* collider = registry_.try_get<engine_collider_component_t>(entity);
    // a disabled collider has no body
    if (collider && collider->disable)
    {
        collider = nullptr;
    }
    if (!registry_.all_of<engine_transform_component_t>(entity) || (!character && !collider))
    {
        return;
    }
    const auto local_to_world = get_world_matrix(entity);
    physcic_internal_component_t physcics_component{};
    if (character)
    {
        if (collider)
        {
            log::log(log::LogLevel::eError, fmt::format("[Entity: {}] has a collider and a character controller, the collider is ignored.\n", static_cast<std::uint32_t>(entity)));
        }
        physcics_component = world_.create_character_body(character->radius, character->height, local_to_world, static_cast<std::int32_t>(entity));
    }
    else
    {
        const auto* rigid_body = without_rigid_body ? nullptr : registry_.try_get<engine_rigid_body_component_t>(entity);
        physcics_component = world_.create_rigid_body(*collider, rigid_body ? get_body_type(entity) : get_collider_body_type(entity),
            rigid_body ? rigid_body->mass : 0.0f, local_to_world, static_cast<std::int32_t>(entity));
    }
    registry_.replace<physcic_internal_component_t>(entity, physcics_component);
    bodies_created_this_frame_.push_back(entity);
}

void engine::PhysicsSystem::sync_bodies_to_transforms(bool force)
{
    if (!force && !transforms_changed_)
    {
        return;
    }
    ENGINE_PROFILE_SECTION;
    transforms_changed_ = false;
    // As a rule of thumb: if rigid body has mass (is dynamic) than it cant be moved by transform component.
    const bool editing = is_editing_();
    const auto move_body = [&](entt::entity entity, bool moved_by_its_transform)
        {
            auto& physcics_component = registry_.get<physcic_internal_component_t>(entity);
            if (!physcics_component.rigid_body)
            {
                return;
            }
            const auto type = get_body_type(entity);
            if (PhysicsWorld::get_body_type(physcics_component.rigid_body) != type)
            {
                // it got or lost a parent or joint attachment: created again, where it is now
                create_body(entity);
                return;
            }
            // moving a static body teleports it: nothing it touches is pushed or woken up. Editing a level does it on purpose.
            if (type == PhysicsWorld::BodyType::eStatic && moved_by_its_transform && !editing && !physcics_component.reported_moving_static
                && std::find(bodies_created_this_frame_.begin(), bodies_created_this_frame_.end(), entity) == bodies_created_this_frame_.end())
            {
                physcics_component.reported_moving_static = true;
                log::log(log::LogLevel::eError, fmt::format("[Entity: {}] A static collider is moved by its transform. If it keeps moving, give it a kinematic rigid body.\n", static_cast<std::uint32_t>(entity)));
            }
            // the body stays where it is when the matrix can't be decomposed (e.g. a zero scale)
            world_.set_world_transform(physcics_component.rigid_body, get_world_matrix(entity));
        };
    // bodies whose world transform depends on other game objects (parents, joints) follow them
    for (const auto entity : registry_.view<const physcic_internal_component_t, const engine_parent_component_t>())
    {
        move_body(entity, false);
    }
    for (const auto entity : registry_.view<const physcic_internal_component_t, const engine_joint_attachment_component_t>(entt::exclude<engine_parent_component_t>))
    {
        move_body(entity, false);
    }
    // the others when their transform changed
    // destroyed game objects and those without a body are filtered out by the view
    for (const auto entity : moved_bodies_.view<physcic_internal_component_t>())
    {
        if (!follows_other_game_objects(entity))
        {
            move_body(entity, true);
        }
    }
    moved_bodies_.clear();
}

void engine::PhysicsSystem::apply_forces()
{
    for (const auto& [entity, force, type] : forces_to_apply_)
    {
        const auto* physcics = registry_.try_get<physcic_internal_component_t>(entity);
        if (!physcics || !physcics->rigid_body)
        {
            continue;
        }
        if (type == ENGINE_FORCE_TYPE_FORCE)
        {
            physcics->rigid_body->applyCentralForce(btVector3(force[0], force[1], force[2]));
        }
        else if (type == ENGINE_FORCE_TYPE_IMPLUSE)
        {
            physcics->rigid_body->applyCentralImpulse(btVector3(force[0], force[1], force[2]));
        }
    }
    forces_to_apply_.clear();
}

void engine::PhysicsSystem::write_dynamic_bodies_to_transforms()
{
    // the bodies are in world space, transforms are local to their parent space
    auto view = registry_.view<engine_transform_component_t, const physcic_internal_component_t, const engine_rigid_body_component_t>();
    view.each([this](auto entity, engine_transform_component_t& transform, const physcic_internal_component_t& physcics, const engine_rigid_body_component_t&)
        {
            if (!physcics.rigid_body)
            {
                return;
            }
            const auto& body_transform = physcics.rigid_body->getWorldTransform();
            const auto& origin = body_transform.getOrigin();
            const auto& body_rotation = body_transform.getRotation();
            auto updated = transform;
            if (!follows_other_game_objects(entity))
            {
                updated.position[0] = origin.getX();
                updated.position[1] = origin.getY();
                updated.position[2] = origin.getZ();
                updated.rotation[0] = body_rotation.getX();
                updated.rotation[1] = body_rotation.getY();
                updated.rotation[2] = body_rotation.getZ();
                updated.rotation[3] = body_rotation.getW();
            }
            else
            {
                // the body has no scale: keep the game object's world scale
                const auto old_world = get_world_matrix(entity);
                const glm::vec3 world_scale{ glm::length(glm::vec3(old_world[0])), glm::length(glm::vec3(old_world[1])), glm::length(glm::vec3(old_world[2])) };
                const glm::vec3 position{ origin.getX(), origin.getY(), origin.getZ() };
                const glm::quat rotation = glm::make_quat(std::array<float, 4>{ body_rotation.getX(), body_rotation.getY(), body_rotation.getZ(), body_rotation.getW() }.data());
                const auto body_world = compute_model_matrix(position, rotation, world_scale);
                // the scale is the game object's, kept exactly
                if (!local_transform_from_world(get_parent_space_matrix(registry_, entity, joint_matrix_), body_world, updated, { .position = true, .rotation = true, .scale = false }))
                {
                    return;
                }
            }
            registry_.replace<engine_transform_component_t>(entity, updated);
        });
}

void engine::PhysicsSystem::on_body_component_changed(entt::registry& registry, entt::entity entity)
{
    if (registry.any_of<physcic_internal_component_t>(entity))
    {
        create_body(entity);
    }
}

void engine::PhysicsSystem::on_rigid_body_removed(entt::registry& registry, entt::entity entity)
{
    // called before the component is gone: the collider alone gets its body
    if (registry.any_of<physcic_internal_component_t>(entity))
    {
        create_body(entity, true, false);
    }
}

void engine::PhysicsSystem::on_character_controller_removed(entt::registry& registry, entt::entity entity)
{
    // called before the component is gone: a collider on the same game object gets its body (without one, the body is
    // removed with the internal component)
    if (registry.any_of<engine_collider_component_t>(entity))
    {
        create_body(entity, false, true);
    }
}

void engine::PhysicsSystem::on_transforms_changed(entt::registry&, entt::entity)
{
    transforms_changed_ = true;
}
