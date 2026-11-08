#include "particle_scene_system.h"
#include "application.h"
#include "data_asset.h"
#include "profiler.h"
#include "scene.h"
#include "world_transform.h"
#include "render/render_scene_data.h"
#include "serialization/builtin_types.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

engine::ParticleSceneSystem::ParticleSceneSystem(Scene& scene, entt::registry& registry)
    : scene_(scene)
    , registry_(registry)
{
    // the particles go with their component
    registry_.on_destroy<engine_particle_emitter_component_t>().connect<&entt::registry::remove<particle_system_internal_component_t>>();
}

const engine::ParticleEffectDesc* engine::ParticleSceneSystem::get_effect(engine_data_asset_t effect) const
{
    const auto* asset = scene_.get_application()->get_data_asset(effect);
    if (!asset || asset->get_type().name != PARTICLE_EFFECT_TYPE_NAME || asset->get_type().size != sizeof(ParticleEffectDesc))
    {
        return nullptr;
    }
    return reinterpret_cast<const ParticleEffectDesc*>(asset->get_value());
}

engine::particle_system_internal_component_t* engine::ParticleSceneSystem::sync(entt::entity entity)
{
    const auto* emitter = registry_.try_get<engine_particle_emitter_component_t>(entity);
    if (!emitter)
    {
        return nullptr;
    }
    auto* internal = registry_.try_get<particle_system_internal_component_t>(entity);
    if (internal && internal->effect == emitter->effect && internal->seed == emitter->seed)
    {
        return internal;
    }
    // a one-shot spawn keeps what it was spawned with
    particle_system_internal_component_t fresh{};
    if (internal)
    {
        fresh.tint = internal->tint;
        fresh.destroy_when_dead = internal->destroy_when_dead;
    }
    fresh.effect = emitter->effect;
    fresh.seed = emitter->seed;
    fresh.system = ParticleSystem(emitter->seed);
    if (emitter->play_on_start && get_effect(emitter->effect))
    {
        fresh.system.play();
    }
    return &registry_.emplace_or_replace<particle_system_internal_component_t>(entity, std::move(fresh));
}

engine::ParticleSimulationParams engine::ParticleSceneSystem::params_of(entt::entity entity) const
{
    ParticleSimulationParams params{};
    if (const auto* world = registry_.try_get<world_transform_internal_component_t>(entity))
    {
        params.world = world->local_to_world;
    }
    params.scale = registry_.get<engine_particle_emitter_component_t>(entity).scale;
    params.gravity = scene_.physics().get_gravity();
    return params;
}

void engine::ParticleSceneSystem::update(float dt)
{
    ENGINE_PROFILE_SECTION_N("particles update");
    const float dt_seconds = dt / 1000.0f;
    std::vector<entt::entity> emitters;
    for (const auto entity : registry_.view<engine_particle_emitter_component_t>())
    {
        emitters.push_back(entity);
    }
    std::vector<entt::entity> dead;
    for (const auto entity : emitters)
    {
        auto* internal = sync(entity);
        const auto* effect = get_effect(internal->effect);
        if (effect)
        {
            internal->system.update(*effect, params_of(entity), dt_seconds);
        }
        // a one-shot spawn whose effect is over (or went) goes, while the scene plays
        if (internal->destroy_when_dead && dt_seconds > 0.0f && (!effect || !internal->system.is_alive()))
        {
            dead.push_back(entity);
        }
    }
    for (const auto entity : dead)
    {
        if (scene_.is_valid_entity(entity))
        {
            scene_.destroy_entity(entity);
        }
    }
}

void engine::ParticleSceneSystem::gather(SceneRenderData& out) const
{
    ENGINE_PROFILE_SECTION_N("particles gather");
    std::vector<ParticleInstance> instances;
    for (const auto [entity, emitter, internal] : registry_.view<const engine_particle_emitter_component_t, const particle_system_internal_component_t>().each())
    {
        const auto* effect = get_effect(internal.effect);
        if (!effect || internal.system.get_count() == 0)
        {
            continue;
        }
        const auto params = params_of(entity);
        const auto emitters = std::min(effect->emitters_count, PARTICLE_EFFECT_MAX_EMITTERS);
        for (std::uint32_t i = 0; i < emitters; i++)
        {
            const auto& state = internal.system.emitter(i);
            if (state.count() == 0)
            {
                continue;
            }
            const auto& desc = effect->emitters[i];
            instances.clear();
            state.gather(desc, params, internal.tint, instances);
            ParticleBatch batch{};
            batch.offset = static_cast<std::uint32_t>(out.particles.size());
            batch.count = static_cast<std::uint32_t>(instances.size());
            // a texture which was released draws as the built-in circle
            batch.texture = desc.texture == ENGINE_INVALID_OBJECT_HANDLE ? nullptr : scene_.get_application()->get_texture(desc.texture);
            batch.blend = static_cast<ParticleBlend>(desc.blend);
            batch.flipbook_columns = std::max(desc.flipbook_columns, 1u);
            batch.flipbook_rows = std::max(desc.flipbook_rows, 1u);
            batch.lit = desc.lit;
            batch.soft_distance = std::max(desc.soft_distance, 0.0f);
            batch.sort_order = desc.sort_order;
            batch.center = glm::vec3(params.world[3]);
            for (const auto& p : instances)
            {
                ParticleGpuData gpu{};
                gpu.position = p.position;
                gpu.size = p.size;
                gpu.color = p.color;
                gpu.rotation = p.rotation;
                gpu.erosion = p.erosion;
                gpu.frame = static_cast<float>(p.flipbook.frame) + p.flipbook.blend;
                gpu.next_frame = static_cast<float>(p.flipbook.next_frame);
                out.particles.push_back(gpu);
            }
            out.particle_batches.push_back(batch);
        }
    }
}

bool engine::ParticleSceneSystem::play(entt::entity entity)
{
    auto* internal = sync(entity);
    if (!internal)
    {
        return false;
    }
    internal->system.play();
    return true;
}

bool engine::ParticleSceneSystem::stop(entt::entity entity, ParticleStopMode mode)
{
    auto* internal = sync(entity);
    if (!internal)
    {
        return false;
    }
    internal->system.stop(mode);
    return true;
}

bool engine::ParticleSceneSystem::is_alive(entt::entity entity) const
{
    const auto* internal = registry_.try_get<particle_system_internal_component_t>(entity);
    if (internal)
    {
        return internal->system.is_alive();
    }
    // not updated yet: alive when it will play
    const auto* emitter = registry_.try_get<engine_particle_emitter_component_t>(entity);
    return emitter && emitter->play_on_start && get_effect(emitter->effect);
}

std::uint32_t engine::ParticleSceneSystem::get_count(entt::entity entity) const
{
    const auto* internal = registry_.try_get<particle_system_internal_component_t>(entity);
    return internal ? internal->system.get_count() : 0;
}

entt::entity engine::ParticleSceneSystem::spawn(engine_data_asset_t effect, const glm::mat4& world, float scale, std::uint32_t seed, const glm::vec4& tint)
{
    if (!get_effect(effect))
    {
        return entt::null;
    }
    const auto entity = scene_.create_transient_entity();
    // the world matrix as a transform (a spawned effect has no parent)
    glm::vec3 world_scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (!glm::decompose(world, world_scale, rotation, translation, skew, perspective))
    {
        world_scale = glm::vec3(1.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        translation = glm::vec3(world[3]);
    }
    auto& transform = registry_.emplace<engine_transform_component_t>(entity);
    std::memcpy(transform.position, &translation[0], sizeof(transform.position));
    std::memcpy(transform.scale, &world_scale[0], sizeof(transform.scale));
    transform.rotation[0] = rotation.x;
    transform.rotation[1] = rotation.y;
    transform.rotation[2] = rotation.z;
    transform.rotation[3] = rotation.w;
    registry_.patch<engine_transform_component_t>(entity);

    auto& emitter = registry_.emplace<engine_particle_emitter_component_t>(entity);
    emitter.effect = effect;
    emitter.play_on_start = true;
    emitter.scale = scale;
    emitter.seed = seed;

    particle_system_internal_component_t internal{};
    internal.effect = effect;
    internal.seed = seed;
    internal.system = ParticleSystem(seed);
    internal.system.play();
    internal.tint = tint;
    internal.destroy_when_dead = true;
    registry_.emplace_or_replace<particle_system_internal_component_t>(entity, std::move(internal));
    // its world matrix is computed with the next update, before it simulates
    if (auto* world_transform = registry_.try_get<world_transform_internal_component_t>(entity))
    {
        world_transform->local_to_world = world;
    }
    return entity;
}

std::uint32_t engine::ParticleSceneSystem::get_live_system_count() const
{
    std::uint32_t count = 0;
    for (const auto [entity, internal] : registry_.view<const particle_system_internal_component_t>().each())
    {
        count += internal.system.is_alive() ? 1 : 0;
    }
    return count;
}

std::uint32_t engine::ParticleSceneSystem::get_live_particle_count() const
{
    std::uint32_t count = 0;
    for (const auto [entity, internal] : registry_.view<const particle_system_internal_component_t>().each())
    {
        count += internal.system.get_count();
    }
    return count;
}
