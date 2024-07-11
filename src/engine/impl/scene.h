#pragma once
#include <engine.h>
#include "graphics.h"

#include "physics_world.h"

#include "material.h"

#include <entt/entt.hpp>

namespace engine
{
class Scene
{
    enum class ShaderType
    {
        eUnlit = 0,
        eLit,
        eVertexSkinningUnlit,
        eVertexSkinningLit,

        eSprite,

        eFullScreenQuad,

        eCount
    };

public:
    Scene(RenderContext& rdx, const engine_scene_create_desc_t& config, engine_result_code_t& out_code);
    Scene(const Scene&) = delete;
    Scene(Scene&& rhs) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&& rhs) = delete;
    ~Scene();

    void enable_physics_debug_draw(bool enable);
    engine_result_code_t update(float dt, std::span<const class Texture2D> textures, 
        std::span<const Geometry> geometries, std::span<const engine_material_create_desc_t> materials,
        std::span<const class NavMesh> nav_meshes, std::span<class Shader> shaders);

    entt::entity create_new_entity();
    void destroy_entity(entt::entity entity);

    entt::runtime_view create_runtime_view();

    std::vector<entt::entity> get_all_entities() const;

    template<typename T>
    void attach_component_to_runtime_view(entt::runtime_view& rv)
    {
        rv.iterate(entity_registry_.storage<T>());
    }

    template<typename T>
    T* add_component(entt::entity entity, T&& component)
    {
        return &entity_registry_.emplace<T>(entity, std::move(component));
    }

    template<typename T>
    T* add_component(entt::entity entity)
    {
        return &entity_registry_.emplace<T>(entity);
    }

    template<typename T>
    const T* get_component(entt::entity entity) const
    {
        return &entity_registry_.get<T>(entity);
    }

    template<typename T>
    void patch_component(entt::entity entity, std::function<void(T&)>&& func)
    {
        entity_registry_.patch<T>(entity, std::move(func));
    }

    template<typename T>
    void update_component(entt::entity entity, const T& t)
    {
        //entity_registry_.patch<T>(entity, [&t](auto& p) { [p = t; ] });
        entity_registry_.replace<T>(entity, t);
    }

    template<typename T>
    void remove_component(entt::entity entity)
    {
        //ToDo: use .remove(entity) instead of erase?
        entity_registry_.erase<T>(entity);
    }

    template<typename T>
    bool has_component(entt::entity entity) const
    {
        return entity_registry_.any_of<T>(entity);
    }

    void set_physcis_gravity(std::array<float, 3> g);
    void get_physcis_collisions_list(const engine_collision_info_t*& ptr_first, size_t* count);
    engine_ray_hit_info_t raycast_into_physics_world(const engine_ray_t& ray, std::span<const engine_game_object_t> ignore_list, float max_distance);

private:
    engine_result_code_t physics_update(float dt);

private:
    RenderContext& rdx_;
    entt::registry entity_registry_;
    entt::observer transform_model_matrix_update_observer;
    entt::observer mesh_update_observer;
    entt::observer collider_create_observer;
    entt::observer collider_update_observer;
    entt::observer transform_update_collider_observer;
    entt::observer rigid_body_create_observer;
    entt::observer rigid_body_update_observer;

    PhysicsWorld physics_world_;

    std::array<Shader, static_cast<std::size_t>(ShaderType::eCount)> shaders_;

    UniformBuffer scene_ubo_;
    ShaderStorageBuffer light_data_ssbo_;

    Framebuffer fbo_;
    Geometry empty_vao_for_full_screen_quad_draw_;

    MaterialStaticGeometryLit material_static_geometry_lit_;
    MaterialSkinnedGeometryLit material_skinned_geometry_lit_;
    MaterialSprite material_sprite_;
};
}  // namespace engine