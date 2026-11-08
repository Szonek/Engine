#pragma once
#include <engine.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

// Particle effects (like Unreal's Niagara systems or Unity's particle systems, simulated on the CPU): an effect is a
// data asset holding a ParticleEffectDesc (the engine's struct type "particle_effect", files under assets/data/effects),
// shared by every emitter component and one-shot spawn using it; a ParticleSystem is one playing instance of it. GL-free
// and unit tested (particles_tests.cpp): what the renderer draws is gathered from the live particles each frame.
namespace engine
{
inline constexpr std::uint32_t PARTICLE_EFFECT_MAX_EMITTERS = 4;
inline constexpr std::uint32_t PARTICLE_EMITTER_MAX_BURSTS = 8;
inline constexpr std::uint32_t PARTICLE_CURVE_MAX_KEYS = 8;
// no emitter holds more live particles than this, whatever its max_particles says
inline constexpr std::uint32_t PARTICLE_EMITTER_CAPACITY_LIMIT = 16384;

// The descriptions are plain C structs (TypeBuilder describes them field by field for data asset files and the
// inspector). Enums are int sized.
enum class ParticleSpawnShape : std::int32_t
{
    ePoint = 0,
    eSphere = 1,
    // a box of half extents shape_half_extents
    eBox = 2,
};

enum class ParticleDirectionMode : std::int32_t
{
    // `direction`, spread by `cone_degrees` around it
    eDirection = 0,
    // away from the shape's centre (a point shape: any direction), spread by `cone_degrees` around that
    eOutward = 1,
};

enum class ParticleSpace : std::int32_t
{
    // particles stay where they spawned
    eWorld = 0,
    // particles move with the emitter
    eLocal = 1,
};

enum class ParticleBlendMode : std::int32_t
{
    eAlpha = 0,
    ePremultiplied = 1,
    eAdditive = 2,
};

struct ParticleRange
{
    float min;
    float max;
};

// the bursts of an emitter are in time order
struct ParticleBurst
{
    // seconds into the emission
    float time;
    std::uint32_t count;
};

struct ParticleCurveKey
{
    // in [0, 1] of the particle's life
    float time;
    float value;
};

// A value over a particle's life: linear between its keys (in time order), the first / last key's value before / after
// them; no keys is 1 everywhere (the start value unchanged).
struct ParticleCurve
{
    std::uint32_t keys_count;
    ParticleCurveKey keys[PARTICLE_CURVE_MAX_KEYS];
};

struct ParticleColorKey
{
    float time;
    float color[3];
};

// like ParticleCurve, with an RGB colour (no keys is white)
struct ParticleColorCurve
{
    std::uint32_t keys_count;
    ParticleColorKey keys[PARTICLE_CURVE_MAX_KEYS];
};

struct ParticleEmitterDesc
{
    bool enabled;

    // emission: `rate` particles a second and the bursts during `duration` seconds, again and again when looping
    float duration;
    bool looping;
    float rate;
    std::uint32_t bursts_count;
    ParticleBurst bursts[PARTICLE_EMITTER_MAX_BURSTS];
    // live particles at most (a spawn beyond it is skipped)
    std::uint32_t max_particles;
    ParticleSpace space;

    // where particles spawn, in the effect's space (scaled by the instance's scale)
    ParticleSpawnShape shape;
    float shape_offset[3];
    float shape_radius;
    float shape_half_extents[3];
    // on the shape's surface instead of inside it
    bool shape_surface;

    // start values, each random in its range
    ParticleRange lifetime;
    ParticleRange speed;
    ParticleDirectionMode direction_mode;
    float direction[3];
    float cone_degrees;
    ParticleRange size;
    ParticleRange rotation_degrees;
    ParticleRange rotation_speed_degrees;
    // RGBA, random between the two
    float color_min[4];
    float color_max[4];

    // over life, multiplying the start values
    ParticleCurve size_over_life;
    ParticleColorCurve color_over_life;
    ParticleCurve alpha_over_life;
    // the alpha threshold rising over life (0..1): the texture's thinner parts vanish first and the particle dissolves
    // instead of fading evenly; no keys: no erosion (unlike the other curves, whose no keys is 1)
    ParticleCurve erosion_over_life;
    // per second, slowing particles down (velocity / (1 + drag * dt))
    float drag;
    // of the scene's gravity
    float gravity_scale;

    // rendering: a camera-facing quad, colour times texture (none: a soft circle)
    engine_texture2d_t texture;
    // the texture is a flipbook of columns x rows frames (left to right, top to bottom), played at flipbook_fps and
    // looping, or once over each particle's life; consecutive frames are blended
    std::uint32_t flipbook_columns;
    std::uint32_t flipbook_rows;
    float flipbook_fps;
    bool flipbook_over_life;
    ParticleBlendMode blend;
    // lit by the scene's lights like a ball of smoke (wrapped diffuse, the directional light's shadow), instead of unlit
    bool lit;
    // a soft particle: faded out where it comes closer than this (world units) to the opaque geometry behind it,
    // so it doesn't cut a hard line into the floor; 0 is off
    float soft_distance;
    // among the effects with the same distance: higher draws later (on top)
    std::int32_t sort_order;
};

struct ParticleEffectDesc
{
    std::uint32_t emitters_count;
    ParticleEmitterDesc emitters[PARTICLE_EFFECT_MAX_EMITTERS];
};

// the values an emitter starts with (a puff that rises and fades for a second), and an effect of one such emitter
ParticleEmitterDesc default_particle_emitter();
ParticleEffectDesc default_particle_effect();

float evaluate_curve(const ParticleCurve& curve, float t);
glm::vec3 evaluate_curve(const ParticleColorCurve& curve, float t);

// The erosion of a particle `t` (0..1) into its life, in [0, 1]: 0 without keys.
float particle_erosion(const ParticleEmitterDesc& desc, float t);

// The flipbook frames a particle shows: `frame` blended into `next_frame` by `blend` (0: only `frame`). Frames are
// numbered left to right, top to bottom; a texture of one frame (or none) is always frame 0.
struct ParticleFlipbookFrame
{
    std::uint32_t frame = 0;
    std::uint32_t next_frame = 0;
    float blend = 0.0f;
};
// Over life: the frames play once over the life, the last one held at its end. Otherwise flipbook_fps frames a second
// from the particle's birth, looping (the last frame blends into the first); 0 fps holds the first frame.
ParticleFlipbookFrame particle_flipbook_frame(const ParticleEmitterDesc& desc, float age, float lifetime);

// SplitMix64 with its own float mapping: the same seed gives the same particles with every compiler and standard
// library (std distributions differ between them).
class ParticleRandom
{
public:
    explicit ParticleRandom(std::uint64_t seed = 0) : state_(seed) {}
    std::uint64_t next_u64()
    {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // in [0, 1): the top 24 bits, so every value is exactly a float
    float next_float() { return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f); }
    // in [min, max)
    float range(float min, float max) { return min + (max - min) * next_float(); }
    float range(const ParticleRange& r) { return range(r.min, r.max); }

private:
    std::uint64_t state_;
};

// What an instance of an effect is simulated with each frame.
struct ParticleSimulationParams
{
    // the emitter's world matrix (its game object's)
    glm::mat4 world = glm::mat4(1.0f);
    // the instance's uniform scale (the component's, times a spawn's)
    float scale = 1.0f;
    // the scene's gravity, world space
    glm::vec3 gravity = glm::vec3(0.0f, -10.0f, 0.0f);
};

// One live particle as the renderer draws it.
struct ParticleInstance
{
    glm::vec3 position;  // world space
    float size;          // world units, the quad's side
    glm::vec4 color;     // RGBA, tint applied
    float rotation;      // radians, around the view axis
    float erosion;       // 0..1, particle_erosion
    ParticleFlipbookFrame flipbook;
};

// The particles of one emitter of a playing instance: structure of arrays, as many slots as its max_particles (no
// allocation per particle; a dead particle is replaced by the last live one).
class ParticleEmitterState
{
public:
    void reset(std::uint64_t seed);
    // emits and moves its particles by `dt` seconds (> 0); `emitting`: whether it may still emit
    void update(const ParticleEmitterDesc& desc, const ParticleSimulationParams& params, float dt, bool emitting);
    // stops emitting; with `clear` its particles go too
    void stop(bool clear);
    std::uint32_t count() const { return count_; }
    // emits nothing any more (its duration is over and it doesn't loop, or it was stopped)
    bool is_finished() const { return finished_; }
    // appends its live particles, world space
    void gather(const ParticleEmitterDesc& desc, const ParticleSimulationParams& params, const glm::vec4& tint, std::vector<ParticleInstance>& out) const;

private:
    void ensure_capacity(std::uint32_t capacity);
    void spawn(const ParticleEmitterDesc& desc, const ParticleSimulationParams& params, std::uint32_t count);
    void kill(std::uint32_t index);

    // per particle (position and velocity in the space of the desc: world, or the emitter's for local)
    std::vector<glm::vec3> position_;
    std::vector<glm::vec3> velocity_;
    std::vector<float> age_;
    std::vector<float> lifetime_;
    std::vector<float> size_;
    std::vector<float> rotation_;
    std::vector<float> rotation_speed_;
    std::vector<glm::vec4> color_;
    std::uint32_t count_ = 0;

    // seconds into the current emission loop, the rate's fractional particle, and the next burst of the loop
    float time_ = 0.0f;
    float rate_accumulator_ = 0.0f;
    std::uint32_t next_burst_ = 0;
    bool finished_ = false;
    ParticleRandom random_;
};

enum class ParticleStopMode
{
    // stop emitting, the live particles live out their lives
    eLiveOut,
    // the live particles go at once
    eClear,
};

// One playing instance of an effect (an emitter component's, or a one-shot spawn's), deterministic from its seed.
class ParticleSystem
{
public:
    ParticleSystem() = default;
    explicit ParticleSystem(std::uint32_t seed) : seed_(seed) {}

    // starts again from the beginning (its particles cleared, the same particles again for the same seed)
    void play();
    void stop(ParticleStopMode mode);
    // simulates `dt` seconds (nothing when it is 0, e.g. while paused)
    void update(const ParticleEffectDesc& effect, const ParticleSimulationParams& params, float dt);

    bool is_playing() const { return playing_; }
    // playing (still emitting) or with live particles
    bool is_alive() const;
    std::uint32_t get_count() const;
    void gather(const ParticleEffectDesc& effect, const ParticleSimulationParams& params, const glm::vec4& tint, std::vector<ParticleInstance>& out) const;
    // the particles of the effect's emitter `index` (< PARTICLE_EFFECT_MAX_EMITTERS), drawn with that emitter's settings
    const ParticleEmitterState& emitter(std::uint32_t index) const { return emitters_[index]; }

private:
    std::uint32_t seed_ = 0;
    bool playing_ = false;
    std::array<ParticleEmitterState, PARTICLE_EFFECT_MAX_EMITTERS> emitters_{};
};

// Back to front by the distance along the camera's forward axis (the farthest first), ties by `sort_order`.
struct ParticleSortKey
{
    float depth;
    std::int32_t sort_order;
    std::uint32_t index;
};
void sort_back_to_front(std::vector<ParticleSortKey>& keys);
}  // namespace engine
