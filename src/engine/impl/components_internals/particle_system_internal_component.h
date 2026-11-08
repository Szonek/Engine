#pragma once
#include "../particles.h"

#include <engine.h>

#include <glm/glm.hpp>

namespace engine
{
// The live particles of a particle emitter component, on its game object: created by the scene's ParticleSceneSystem
// for the component's effect and seed (again when either changes), destroyed with the component or the game object.
// Derived data, never saved.
struct particle_system_internal_component_t
{
    // what the system was created for
    engine_data_asset_t effect = ENGINE_INVALID_OBJECT_HANDLE;
    std::uint32_t seed = 0;
    ParticleSystem system;
    // a one-shot spawn (engineParticlesSpawn): its colour tint, and its game object goes when its last particle dies
    glm::vec4 tint = glm::vec4(1.0f);
    bool destroy_when_dead = false;
};
}  // namespace engine
