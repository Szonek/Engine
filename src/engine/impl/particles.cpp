#include "particles.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float DEGREES_TO_RADIANS = 3.14159265358979f / 180.0f;
// a random unit vector inside a cone is found by rejection sampling; after this many misses the cone's axis is used
constexpr int MAX_SAMPLE_TRIES = 16;

// a point in the unit disk (x, y) and its squared length; false after too many misses
bool sample_unit_disk(engine::ParticleRandom& random, float& x, float& y, float& length_squared)
{
    for (int i = 0; i < MAX_SAMPLE_TRIES; i++)
    {
        x = random.range(-1.0f, 1.0f);
        y = random.range(-1.0f, 1.0f);
        length_squared = x * x + y * y;
        if (length_squared <= 1.0f && length_squared > 1e-8f)
        {
            return true;
        }
    }
    return false;
}

// a point in the unit ball (or on the unit sphere)
glm::vec3 sample_unit_ball(engine::ParticleRandom& random, bool surface)
{
    for (int i = 0; i < MAX_SAMPLE_TRIES; i++)
    {
        const glm::vec3 p(random.range(-1.0f, 1.0f), random.range(-1.0f, 1.0f), random.range(-1.0f, 1.0f));
        const float length_squared = glm::dot(p, p);
        if (length_squared <= 1.0f && length_squared > 1e-8f)
        {
            return surface ? p / std::sqrt(length_squared) : p;
        }
    }
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

// any two unit vectors perpendicular to `axis` (a unit vector) and to each other
void basis_around(const glm::vec3& axis, glm::vec3& u, glm::vec3& v)
{
    const glm::vec3 helper = std::abs(axis.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    u = glm::normalize(glm::cross(helper, axis));
    v = glm::cross(axis, u);
}

// a unit vector within `cos_half_angle` of `axis`, uniform over the cone's cap: its height along the axis is uniform,
// its angle around it comes from a point of the unit disk (no trigonometry per particle)
glm::vec3 sample_cone(engine::ParticleRandom& random, const glm::vec3& axis, float cos_half_angle)
{
    const float z = cos_half_angle + (1.0f - cos_half_angle) * random.next_float();
    float x = 0.0f;
    float y = 0.0f;
    float length_squared = 0.0f;
    if (!sample_unit_disk(random, x, y, length_squared))
    {
        return axis;
    }
    const float side = std::sqrt(std::max(0.0f, 1.0f - z * z)) / std::sqrt(length_squared);
    glm::vec3 u;
    glm::vec3 v;
    basis_around(axis, u, v);
    return axis * z + u * (x * side) + v * (y * side);
}

glm::vec3 sample_box(engine::ParticleRandom& random, const glm::vec3& half, bool surface)
{
    glm::vec3 p(random.range(-1.0f, 1.0f), random.range(-1.0f, 1.0f), random.range(-1.0f, 1.0f));
    if (surface)
    {
        // a face, by its area: the pairs of faces normal to x, y and z
        const float area_x = half.y * half.z;
        const float area_y = half.x * half.z;
        const float area_z = half.x * half.y;
        const float total = area_x + area_y + area_z;
        const float pick = random.next_float() * total;
        const float sign = random.next_float() < 0.5f ? -1.0f : 1.0f;
        if (total <= 0.0f || pick < area_x)
        {
            p.x = sign;
        }
        else if (pick < area_x + area_y)
        {
            p.y = sign;
        }
        else
        {
            p.z = sign;
        }
    }
    return p * half;
}

glm::vec3 to_vec3(const float v[3])
{
    return glm::vec3(v[0], v[1], v[2]);
}

glm::vec4 to_vec4(const float v[4])
{
    return glm::vec4(v[0], v[1], v[2], v[3]);
}

// the uniform scale of a world matrix (its first axis)
float world_scale_of(const glm::mat4& world)
{
    return glm::length(glm::vec3(world[0]));
}
}  // namespace

engine::ParticleEmitterDesc engine::default_particle_emitter()
{
    ParticleEmitterDesc e{};
    e.enabled = true;
    e.duration = 1.0f;
    e.looping = true;
    e.rate = 10.0f;
    e.max_particles = 100;
    e.space = ParticleSpace::eWorld;
    e.shape = ParticleSpawnShape::ePoint;
    e.shape_radius = 0.5f;
    e.shape_half_extents[0] = e.shape_half_extents[1] = e.shape_half_extents[2] = 0.5f;
    e.lifetime = { 1.0f, 1.0f };
    e.speed = { 1.0f, 1.0f };
    e.direction_mode = ParticleDirectionMode::eDirection;
    e.direction[1] = 1.0f;
    e.cone_degrees = 15.0f;
    e.size = { 0.25f, 0.25f };
    for (int i = 0; i < 4; i++)
    {
        e.color_min[i] = 1.0f;
        e.color_max[i] = 1.0f;
    }
    // fades out over its life
    e.alpha_over_life.keys_count = 2;
    e.alpha_over_life.keys[0] = { 0.0f, 1.0f };
    e.alpha_over_life.keys[1] = { 1.0f, 0.0f };
    e.gravity_scale = 0.0f;
    e.flipbook_columns = 1;
    e.flipbook_rows = 1;
    e.flipbook_fps = 10.0f;
    e.blend = ParticleBlendMode::eAlpha;
    e.texture_channels = ParticleTextureChannels::eRgba;
    e.erosion_softness = 0.1f;
    e.light_wrap = 0.5f;
    return e;
}

engine::ParticleEffectDesc engine::default_particle_effect()
{
    ParticleEffectDesc effect{};
    effect.emitters_count = 1;
    for (auto& e : effect.emitters)
    {
        e = default_particle_emitter();
    }
    return effect;
}

float engine::evaluate_curve(const ParticleCurve& curve, float t, float empty)
{
    const auto count = std::min(curve.keys_count, PARTICLE_CURVE_MAX_KEYS);
    if (count == 0)
    {
        return empty;
    }
    if (t <= curve.keys[0].time)
    {
        return curve.keys[0].value;
    }
    for (std::uint32_t i = 1; i < count; i++)
    {
        const auto& a = curve.keys[i - 1];
        const auto& b = curve.keys[i];
        if (t <= b.time)
        {
            const float span = b.time - a.time;
            return span > 0.0f ? a.value + (b.value - a.value) * ((t - a.time) / span) : b.value;
        }
    }
    return curve.keys[count - 1].value;
}

glm::vec3 engine::evaluate_curve(const ParticleColorCurve& curve, float t)
{
    const auto count = std::min(curve.keys_count, PARTICLE_CURVE_MAX_KEYS);
    if (count == 0)
    {
        return glm::vec3(1.0f);
    }
    if (t <= curve.keys[0].time)
    {
        return to_vec3(curve.keys[0].color);
    }
    for (std::uint32_t i = 1; i < count; i++)
    {
        const auto& a = curve.keys[i - 1];
        const auto& b = curve.keys[i];
        if (t <= b.time)
        {
            const float span = b.time - a.time;
            const float f = span > 0.0f ? (t - a.time) / span : 1.0f;
            return to_vec3(a.color) + (to_vec3(b.color) - to_vec3(a.color)) * f;
        }
    }
    return to_vec3(curve.keys[count - 1].color);
}

float engine::flipbook_frame(const ParticleEmitterDesc& desc, float age, float lifetime)
{
    const auto frames = std::max(1u, desc.flipbook_columns) * std::max(1u, desc.flipbook_rows);
    if (frames == 1)
    {
        return 0.0f;
    }
    const auto count = static_cast<float>(frames);
    if (desc.flipbook_over_life)
    {
        // once: the first frame at the start, the last one at the end
        const float t = lifetime > 0.0f ? std::clamp(age / lifetime, 0.0f, 1.0f) : 0.0f;
        return desc.flipbook_blend ? t * (count - 1.0f) : std::min(std::floor(t * count), count - 1.0f);
    }
    // looping at its fps
    float frame = std::max(0.0f, age * desc.flipbook_fps);
    frame -= std::floor(frame / count) * count;
    return desc.flipbook_blend ? frame : std::min(std::floor(frame), count - 1.0f);
}

void engine::ParticleEmitterState::reset(std::uint64_t seed)
{
    count_ = 0;
    time_ = 0.0f;
    rate_accumulator_ = 0.0f;
    next_burst_ = 0;
    finished_ = false;
    random_ = ParticleRandom(seed);
}

void engine::ParticleEmitterState::stop(bool clear)
{
    finished_ = true;
    if (clear)
    {
        count_ = 0;
    }
}

void engine::ParticleEmitterState::ensure_capacity(std::uint32_t capacity)
{
    if (position_.size() >= capacity)
    {
        return;
    }
    position_.resize(capacity);
    velocity_.resize(capacity);
    age_.resize(capacity);
    lifetime_.resize(capacity);
    size_.resize(capacity);
    rotation_.resize(capacity);
    rotation_speed_.resize(capacity);
    color_.resize(capacity);
}

void engine::ParticleEmitterState::kill(std::uint32_t index)
{
    const auto last = count_ - 1;
    position_[index] = position_[last];
    velocity_[index] = velocity_[last];
    age_[index] = age_[last];
    lifetime_[index] = lifetime_[last];
    size_[index] = size_[last];
    rotation_[index] = rotation_[last];
    rotation_speed_[index] = rotation_speed_[last];
    color_[index] = color_[last];
    count_ = last;
}

void engine::ParticleEmitterState::spawn(const ParticleEmitterDesc& desc, const ParticleSimulationParams& params, std::uint32_t count)
{
    const auto capacity = std::min(desc.max_particles, PARTICLE_EMITTER_CAPACITY_LIMIT);
    ensure_capacity(capacity);
    const float cos_half = std::cos(std::clamp(desc.cone_degrees, 0.0f, 180.0f) * DEGREES_TO_RADIANS);
    glm::vec3 axis = to_vec3(desc.direction);
    axis = glm::dot(axis, axis) > 1e-12f ? glm::normalize(axis) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 offset = to_vec3(desc.shape_offset);
    const glm::vec3 half = glm::max(to_vec3(desc.shape_half_extents), glm::vec3(0.0f));
    const glm::mat3 world_basis(params.world);
    for (std::uint32_t n = 0; n < count && count_ < capacity; n++)
    {
        // in the effect's space, unscaled
        glm::vec3 local(0.0f);
        switch (desc.shape)
        {
        case ParticleSpawnShape::ePoint: break;
        case ParticleSpawnShape::eSphere: local = sample_unit_ball(random_, desc.shape_surface) * std::max(0.0f, desc.shape_radius); break;
        case ParticleSpawnShape::eBox: local = sample_box(random_, half, desc.shape_surface); break;
        }
        glm::vec3 direction_axis = axis;
        if (desc.direction_mode == ParticleDirectionMode::eOutward)
        {
            direction_axis = glm::dot(local, local) > 1e-12f ? glm::normalize(local) : sample_unit_ball(random_, true);
        }
        const glm::vec3 direction = sample_cone(random_, direction_axis, cos_half);
        const float speed = random_.range(desc.speed);
        // the instance's scale scales the shape, the speeds and the sizes
        const glm::vec3 local_position = (offset + local) * params.scale;
        const glm::vec3 local_velocity = direction * (speed * params.scale);

        const auto i = count_++;
        if (desc.space == ParticleSpace::eWorld)
        {
            position_[i] = glm::vec3(params.world * glm::vec4(local_position, 1.0f));
            velocity_[i] = world_basis * local_velocity;
        }
        else
        {
            position_[i] = local_position;
            velocity_[i] = local_velocity;
        }
        age_[i] = 0.0f;
        lifetime_[i] = std::max(1e-3f, random_.range(desc.lifetime));
        size_[i] = random_.range(desc.size) * params.scale;
        rotation_[i] = random_.range(desc.rotation_degrees) * DEGREES_TO_RADIANS;
        rotation_speed_[i] = random_.range(desc.rotation_speed_degrees) * DEGREES_TO_RADIANS;
        const glm::vec4 a = to_vec4(desc.color_min);
        const glm::vec4 b = to_vec4(desc.color_max);
        color_[i] = a + (b - a) * random_.next_float();
    }
}

void engine::ParticleEmitterState::update(const ParticleEmitterDesc& desc, const ParticleSimulationParams& params, float dt, bool emitting)
{
    // a smaller max_particles drops the newest beyond it
    count_ = std::min(count_, std::min(desc.max_particles, PARTICLE_EMITTER_CAPACITY_LIMIT));

    // the live ones age and move
    glm::vec3 gravity = params.gravity * desc.gravity_scale;
    if (desc.space == ParticleSpace::eLocal)
    {
        // into the emitter's space (where local particles move)
        const glm::mat3 basis(params.world);
        gravity = glm::transpose(basis) * gravity / std::max(1e-12f, glm::dot(basis[0], basis[0]));
    }
    const float damping = 1.0f / (1.0f + std::max(0.0f, desc.drag) * dt);
    for (std::uint32_t i = 0; i < count_;)
    {
        age_[i] += dt;
        if (age_[i] >= lifetime_[i])
        {
            kill(i);
            continue;
        }
        velocity_[i] = (velocity_[i] + gravity * dt) * damping;
        position_[i] += velocity_[i] * dt;
        rotation_[i] += rotation_speed_[i] * dt;
        i++;
    }

    if (!emitting || finished_ || !desc.enabled)
    {
        return;
    }
    // then this frame's new ones: the bursts in [time, time + dt) and the rate, over the loops the frame covers
    const float duration = std::max(0.0f, desc.duration);
    const auto bursts = std::min(desc.bursts_count, PARTICLE_EMITTER_MAX_BURSTS);
    float remaining = dt;
    // a frame longer than a loop plays at most a few loops' worth
    for (int loops = 0; loops < 4 && remaining > 0.0f; loops++)
    {
        const float end = time_ + remaining;
        const float emit_end = std::min(end, duration);
        while (next_burst_ < bursts && desc.bursts[next_burst_].time < end && desc.bursts[next_burst_].time <= duration)
        {
            spawn(desc, params, desc.bursts[next_burst_].count);
            next_burst_++;
        }
        if (emit_end > time_)
        {
            rate_accumulator_ += std::max(0.0f, desc.rate) * (emit_end - time_);
            const auto whole = static_cast<std::uint32_t>(rate_accumulator_);
            rate_accumulator_ -= static_cast<float>(whole);
            spawn(desc, params, whole);
        }
        if (end < duration)
        {
            time_ = end;
            return;
        }
        // the end of the emission: again from the start when looping
        if (!desc.looping || duration <= 0.0f)
        {
            finished_ = true;
            return;
        }
        remaining = end - duration;
        time_ = 0.0f;
        next_burst_ = 0;
    }
    time_ = std::min(time_ + remaining, duration);
}

void engine::ParticleEmitterState::gather(const ParticleEmitterDesc& desc, const ParticleSimulationParams& params, const glm::vec4& tint, std::vector<ParticleInstance>& out) const
{
    const bool local = desc.space == ParticleSpace::eLocal;
    // local particles are drawn in the emitter's space, whose scale scales them too
    const float size_scale = local ? world_scale_of(params.world) : 1.0f;
    for (std::uint32_t i = 0; i < count_; i++)
    {
        const float t = age_[i] / lifetime_[i];
        ParticleInstance p{};
        p.position = local ? glm::vec3(params.world * glm::vec4(position_[i], 1.0f)) : position_[i];
        p.size = size_[i] * size_scale * evaluate_curve(desc.size_over_life, t);
        const glm::vec3 rgb = glm::vec3(color_[i]) * evaluate_curve(desc.color_over_life, t);
        p.color = glm::vec4(rgb, color_[i].a * evaluate_curve(desc.alpha_over_life, t)) * tint;
        p.rotation = rotation_[i];
        p.erosion = std::clamp(evaluate_curve(desc.erosion_over_life, t, 0.0f), 0.0f, 1.0f);
        p.frame = flipbook_frame(desc, age_[i], lifetime_[i]);
        out.push_back(p);
    }
}

void engine::ParticleSystem::play()
{
    playing_ = true;
    for (std::uint32_t i = 0; i < emitters_.size(); i++)
    {
        // each emitter its own sequence, the same for the same seed
        emitters_[i].reset(ParticleRandom(seed_ * 0x100000001B3ull + i + 1).next_u64());
    }
}

void engine::ParticleSystem::stop(ParticleStopMode mode)
{
    playing_ = false;
    for (auto& emitter : emitters_)
    {
        emitter.stop(mode == ParticleStopMode::eClear);
    }
}

void engine::ParticleSystem::update(const ParticleEffectDesc& effect, const ParticleSimulationParams& params, float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    const auto count = std::min(effect.emitters_count, PARTICLE_EFFECT_MAX_EMITTERS);
    bool emitting = false;
    for (std::uint32_t i = 0; i < emitters_.size(); i++)
    {
        auto& emitter = emitters_[i];
        if (i >= count)
        {
            // an emitter the effect doesn't have (any more)
            emitter.stop(true);
            continue;
        }
        emitter.update(effect.emitters[i], params, dt, playing_);
        emitting = emitting || (!emitter.is_finished() && effect.emitters[i].enabled);
    }
    // every emitter is done: it lives on only while its particles do
    if (!emitting)
    {
        playing_ = false;
    }
}

bool engine::ParticleSystem::is_alive() const
{
    return playing_ || get_count() > 0;
}

std::uint32_t engine::ParticleSystem::get_count() const
{
    std::uint32_t count = 0;
    for (const auto& emitter : emitters_)
    {
        count += emitter.count();
    }
    return count;
}

void engine::ParticleSystem::gather(const ParticleEffectDesc& effect, const ParticleSimulationParams& params, const glm::vec4& tint, std::vector<ParticleInstance>& out) const
{
    const auto count = std::min(effect.emitters_count, PARTICLE_EFFECT_MAX_EMITTERS);
    for (std::uint32_t i = 0; i < count; i++)
    {
        emitters_[i].gather(effect.emitters[i], params, tint, out);
    }
}

void engine::sort_back_to_front(std::vector<ParticleSortKey>& keys)
{
    std::sort(keys.begin(), keys.end(), [](const ParticleSortKey& a, const ParticleSortKey& b) {
        if (a.depth != b.depth)
        {
            return a.depth > b.depth;
        }
        if (a.sort_order != b.sort_order)
        {
            return a.sort_order < b.sort_order;
        }
        return a.index < b.index;
    });
}
