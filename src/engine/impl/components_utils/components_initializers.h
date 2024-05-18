#pragma once
#include "entt/entt.hpp"


namespace engine
{
void initialize_transform_component(entt::registry& registry, entt::entity entity);
void initialize_mesh_component(entt::registry& registry, entt::entity entity);
void initialize_material_component(entt::registry& registry, entt::entity entity);
void initialize_parent_component(entt::registry& registry, entt::entity entity);
void initialize_name_component(entt::registry& registry, entt::entity entity);
void initialize_camera_component(entt::registry& registry, entt::entity entity);
void initialize_rigidbody_component(entt::registry& registry, entt::entity entity);
void initialize_collider_component(entt::registry& registry, entt::entity entity);
void initialize_skin_component(entt::registry& registry, entt::entity entity);
void initialize_light_component(entt::registry& registry, entt::entity entity);
} // namespace engine