#pragma once
#include "particles.h"
#include "components_internals/particle_system_internal_component.h"

#include <engine.h>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>

namespace engine
{
class Application;
class Scene;
struct SceneRenderData;

// The particles of a scene (like PhysicsSystem for physics): plays a ParticleSystem for every particle emitter
// component (particle_system_internal_component_t), simulates them after the world transforms each update, spawns the
// one-shot effects of engineParticlesSpawn (transient game objects destroyed when their last particle dies) and gathers
// what the Transparent pass draws. The effect is read from its data asset on every update, so an edit (the inspector, a
// hot reload) shows at once. Frozen while the scene steps with dt 0 (paused, Edit mode); a scene replaced at Stop drops
// every system with its game object.
class ParticleSceneSystem
{
public:
    // Connects to the registry's signals.
    ParticleSceneSystem(Scene& scene, entt::registry& registry);
    ParticleSceneSystem(const ParticleSceneSystem&) = delete;
    ParticleSceneSystem& operator=(const ParticleSceneSystem&) = delete;

    // dt in milliseconds; after the world transforms
    void update(float dt);
    // the live particles into `out`, a batch per emitter with particles
    void gather(SceneRenderData& out) const;

    // see engineParticles*: false when the game object has no emitter component
    bool play(entt::entity entity);
    bool stop(entt::entity entity, ParticleStopMode mode);
    bool is_alive(entt::entity entity) const;
    std::uint32_t get_count(entt::entity entity) const;
    // see engineParticlesSpawn: entt::null when `effect` holds no particle effect
    entt::entity spawn(engine_data_asset_t effect, const glm::mat4& world, float scale, std::uint32_t seed, const glm::vec4& tint);

    // the effect a data asset holds, nullptr when it holds something else (or nothing)
    const ParticleEffectDesc* get_effect(engine_data_asset_t effect) const;

    // for the editor's stats
    std::uint32_t get_live_system_count() const;
    std::uint32_t get_live_particle_count() const;

private:
    // the internal component, created (or created again, when the effect or seed changed) for the emitter component;
    // a new one plays when the component plays on start and has an effect
    particle_system_internal_component_t* sync(entt::entity entity);
    ParticleSimulationParams params_of(entt::entity entity) const;

    Scene& scene_;
    entt::registry& registry_;
};
}  // namespace engine
