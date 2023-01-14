#pragma once
#include <engine.h>
#include "graphics.h"

#include <entt/entt.hpp>

namespace engine
{
class Scene
{

public:
    Scene(engine_result_code_t& out_code);
    Scene(const Scene&) = delete;
    Scene(Scene&& rhs) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&& rhs) = delete;
    ~Scene();

    engine_result_code_t update(RenderContext& rdx, float dt, std::span<const class Texture2D> textures, std::span<const Geometry> geometries);

    entt::entity create_new_entity();
    void destroy_entity(entt::entity entity);

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
    T* get_component(entt::entity entity)
    {
        return &entity_registry_.get<T>(entity);
    }

    template<typename T>
    void remove_component(entt::entity entity)
    {
        //ToDo: use .remove(entity) instead of erase?
        entity_registry_.erase<T>(entity);
    }

    template<typename T>
    bool has_component(entt::entity entity)
    {
        return entity_registry_.any_of<T>(entity);
    }

private:
    entt::registry entity_registry_;
    Shader shader_;
};
}  // namespace engine