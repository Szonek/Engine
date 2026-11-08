#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_data_asset_t;

// A particle effect playing on its game object (like Unity's ParticleSystem component or Unreal's Niagara component):
// level content such as a torch's flame or dust. `effect` is a data asset holding a particle effect (a file under
// assets/data/effects, see engineDataAssetLoad; a scene file referencing one holds it). The particles are derived data,
// never saved; they follow the game object's world matrix. A new component plays on start, at scale 1, seed 0, with no
// effect.
typedef struct _engine_particle_emitter_component_t
{
    engine_data_asset_t effect;
    // plays as soon as it has an effect (else it waits for engineParticlesPlay)
    bool play_on_start;
    // uniform: the spawn shape, speeds and sizes
    float scale;
    // the same seed gives the same particles
    uint32_t seed;
} engine_particle_emitter_component_t;

#ifdef __cplusplus
}
#endif // cpp
