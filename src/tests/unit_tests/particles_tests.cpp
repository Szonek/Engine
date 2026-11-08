#include "particles.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

// The particle simulation (impl/particles.h): emission timing, bursts, rate, capacity, lifetimes, curves, determinism,
// world and local space, stopping and death.
namespace
{
using engine::ParticleEffectDesc;
using engine::ParticleEmitterDesc;
using engine::ParticleInstance;
using engine::ParticleSimulationParams;
using engine::ParticleSystem;

constexpr float DT = 1.0f / 60.0f;

// one emitter with nothing but what a test sets: no rate, no bursts, long-lived particles that don't move
ParticleEffectDesc quiet_effect()
{
    auto effect = engine::default_particle_effect();
    auto& e = effect.emitters[0];
    e.rate = 0.0f;
    e.looping = false;
    e.duration = 1.0f;
    e.lifetime = { 10.0f, 10.0f };
    e.speed = { 0.0f, 0.0f };
    e.alpha_over_life.keys_count = 0;
    return effect;
}

void step(ParticleSystem& system, const ParticleEffectDesc& effect, std::uint32_t frames, const ParticleSimulationParams& params = {})
{
    for (std::uint32_t i = 0; i < frames; i++)
    {
        system.update(effect, params, DT);
    }
}

std::vector<ParticleInstance> gather(const ParticleSystem& system, const ParticleEffectDesc& effect, const ParticleSimulationParams& params = {})
{
    std::vector<ParticleInstance> out;
    system.gather(effect, params, glm::vec4(1.0f), out);
    return out;
}
}  // namespace

TEST(ParticleCurve, NoKeysIsOne)
{
    engine::ParticleCurve curve{};
    EXPECT_EQ(engine::evaluate_curve(curve, 0.3f), 1.0f);
    engine::ParticleColorCurve colors{};
    EXPECT_EQ(engine::evaluate_curve(colors, 0.3f), glm::vec3(1.0f));
}

TEST(ParticleCurve, LinearBetweenKeysClampedOutside)
{
    engine::ParticleCurve curve{};
    curve.keys_count = 3;
    curve.keys[0] = { 0.2f, 1.0f };
    curve.keys[1] = { 0.6f, 3.0f };
    curve.keys[2] = { 1.0f, 0.0f };
    EXPECT_FLOAT_EQ(engine::evaluate_curve(curve, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(engine::evaluate_curve(curve, 0.4f), 2.0f);
    EXPECT_FLOAT_EQ(engine::evaluate_curve(curve, 0.8f), 1.5f);
    EXPECT_FLOAT_EQ(engine::evaluate_curve(curve, 1.0f), 0.0f);
}

TEST(ParticleCurve, ColorLerps)
{
    engine::ParticleColorCurve curve{};
    curve.keys_count = 2;
    curve.keys[0] = { 0.0f, { 1.0f, 0.0f, 0.0f } };
    curve.keys[1] = { 1.0f, { 0.0f, 0.0f, 1.0f } };
    const auto mid = engine::evaluate_curve(curve, 0.5f);
    EXPECT_FLOAT_EQ(mid.r, 0.5f);
    EXPECT_FLOAT_EQ(mid.b, 0.5f);
}

TEST(ParticleSystem, NothingBeforePlay)
{
    auto effect = quiet_effect();
    effect.emitters[0].bursts_count = 1;
    effect.emitters[0].bursts[0] = { 0.0f, 5 };
    ParticleSystem system(1);
    step(system, effect, 10);
    EXPECT_EQ(system.get_count(), 0u);
    EXPECT_FALSE(system.is_alive());
}

TEST(ParticleSystem, BurstsFireAtTheirTime)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.bursts_count = 2;
    e.bursts[0] = { 0.0f, 5 };
    e.bursts[1] = { 0.505f, 7 };
    ParticleSystem system(1);
    system.play();
    step(system, effect, 1);
    EXPECT_EQ(system.get_count(), 5u);
    // 0.505 s is in the 31st frame ([30, 31) * DT)
    step(system, effect, 29);
    EXPECT_EQ(system.get_count(), 5u);
    step(system, effect, 1);
    EXPECT_EQ(system.get_count(), 12u);
}

TEST(ParticleSystem, RateEmitsOverTheDuration)
{
    auto effect = quiet_effect();
    effect.emitters[0].rate = 30.0f;
    effect.emitters[0].duration = 2.0f;
    ParticleSystem system(1);
    system.play();
    step(system, effect, 60);
    EXPECT_NEAR(static_cast<float>(system.get_count()), 30.0f, 1.0f);
    step(system, effect, 120);
    // nothing after the duration
    EXPECT_NEAR(static_cast<float>(system.get_count()), 60.0f, 1.0f);
}

TEST(ParticleSystem, LoopingBurstsAgainEachLoop)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.looping = true;
    e.duration = 0.5f;
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 3 };
    ParticleSystem system(1);
    system.play();
    // loops start at 0, 0.5, 1.0 and 1.5 s: four bursts in 1.6 s
    step(system, effect, 96);
    EXPECT_EQ(system.get_count(), 12u);
    EXPECT_TRUE(system.is_playing());
}

TEST(ParticleSystem, CapacityLimitsLiveParticles)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.max_particles = 10;
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 50 };
    ParticleSystem system(1);
    system.play();
    step(system, effect, 2);
    EXPECT_EQ(system.get_count(), 10u);
}

TEST(ParticleSystem, ParticlesDieAfterTheirLifetime)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.lifetime = { 0.5f, 0.5f };
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 4 };
    ParticleSystem system(1);
    system.play();
    step(system, effect, 29);
    EXPECT_EQ(system.get_count(), 4u);
    step(system, effect, 2);
    EXPECT_EQ(system.get_count(), 0u);
    // a one-shot effect whose emission is over and whose particles are gone is dead
    step(system, effect, 60);
    EXPECT_FALSE(system.is_alive());
}

TEST(ParticleSystem, SameSeedSameParticles)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.shape = engine::ParticleSpawnShape::eSphere;
    e.shape_radius = 2.0f;
    e.speed = { 1.0f, 3.0f };
    e.cone_degrees = 60.0f;
    e.size = { 0.1f, 1.0f };
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 20 };
    ParticleSystem a(42);
    ParticleSystem b(42);
    ParticleSystem c(43);
    for (auto* s : { &a, &b, &c })
    {
        s->play();
        step(*s, effect, 10);
    }
    const auto pa = gather(a, effect);
    const auto pb = gather(b, effect);
    const auto pc = gather(c, effect);
    ASSERT_EQ(pa.size(), 20u);
    ASSERT_EQ(pb.size(), 20u);
    bool any_different = false;
    for (std::size_t i = 0; i < pa.size(); i++)
    {
        EXPECT_EQ(pa[i].position, pb[i].position);
        EXPECT_EQ(pa[i].size, pb[i].size);
        any_different = any_different || pa[i].position != pc[i].position;
    }
    EXPECT_TRUE(any_different);

    // playing again gives the same particles again
    a.play();
    step(a, effect, 10);
    const auto again = gather(a, effect);
    ASSERT_EQ(again.size(), pa.size());
    EXPECT_EQ(again[0].position, pa[0].position);
}

TEST(ParticleSystem, SpawnShapesAndConesStayWithinTheirBounds)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 200 };
    e.max_particles = 200;
    ParticleSystem system(7);

    e.shape = engine::ParticleSpawnShape::eSphere;
    e.shape_radius = 2.0f;
    e.shape_surface = true;
    system.play();
    step(system, effect, 1);
    for (const auto& p : gather(system, effect))
    {
        EXPECT_NEAR(glm::length(p.position), 2.0f, 1e-3f);
    }

    e.shape = engine::ParticleSpawnShape::eBox;
    e.shape_surface = false;
    e.shape_half_extents[0] = 1.0f;
    e.shape_half_extents[1] = 0.5f;
    e.shape_half_extents[2] = 0.25f;
    system.play();
    step(system, effect, 1);
    for (const auto& p : gather(system, effect))
    {
        EXPECT_LE(std::abs(p.position.x), 1.0f);
        EXPECT_LE(std::abs(p.position.y), 0.5f);
        EXPECT_LE(std::abs(p.position.z), 0.25f);
    }

    // point shape, a 30 degree cone around +x: the particles move within it
    e.shape = engine::ParticleSpawnShape::ePoint;
    e.speed = { 1.0f, 1.0f };
    e.direction[0] = 1.0f;
    e.direction[1] = 0.0f;
    e.cone_degrees = 30.0f;
    system.play();
    step(system, effect, 2);
    const float cos_cone = std::cos(30.0f * 3.14159265f / 180.0f);
    for (const auto& p : gather(system, effect))
    {
        ASSERT_GT(glm::length(p.position), 0.0f);
        EXPECT_GE(glm::normalize(p.position).x, cos_cone - 1e-3f);
    }
}

TEST(ParticleSystem, WorldParticlesStayLocalOnesFollowTheEmitter)
{
    auto effect = quiet_effect();
    effect.emitters[0].bursts_count = 1;
    effect.emitters[0].bursts[0] = { 0.0f, 1 };
    auto local_effect = effect;
    local_effect.emitters[0].space = engine::ParticleSpace::eLocal;

    ParticleSimulationParams params{};
    params.world = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ParticleSystem world(1);
    ParticleSystem local(1);
    world.play();
    local.play();
    step(world, effect, 1, params);
    step(local, local_effect, 1, params);
    // the emitter moves on
    params.world = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f));
    step(world, effect, 1, params);
    step(local, local_effect, 1, params);
    EXPECT_NEAR(gather(world, effect, params)[0].position.x, 1.0f, 1e-4f);
    EXPECT_NEAR(gather(local, local_effect, params)[0].position.x, 5.0f, 1e-4f);
}

TEST(ParticleSystem, GravityDragAndScale)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 1 };
    e.gravity_scale = 1.0f;
    ParticleSystem falling(1);
    falling.play();
    step(falling, effect, 61);
    // about 1 s of the default (0, -10, 0) gravity: ~5 m
    EXPECT_NEAR(gather(falling, effect)[0].position.y, -5.0f, 0.2f);

    e.gravity_scale = 0.0f;
    e.speed = { 2.0f, 2.0f };
    e.cone_degrees = 0.0f;
    e.drag = 100.0f;
    ParticleSystem dragged(1);
    dragged.play();
    step(dragged, effect, 61);
    EXPECT_LT(gather(dragged, effect)[0].position.y, 0.2f);

    // the scale scales sizes and the shape
    e.drag = 0.0f;
    e.speed = { 0.0f, 0.0f };
    e.size = { 0.5f, 0.5f };
    ParticleSimulationParams params{};
    params.scale = 3.0f;
    ParticleSystem scaled(1);
    scaled.play();
    step(scaled, effect, 1, params);
    EXPECT_FLOAT_EQ(gather(scaled, effect, params)[0].size, 1.5f);
}

TEST(ParticleSystem, OverLifeCurvesAndTint)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.lifetime = { 1.0f, 1.0f };
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 1 };
    e.size = { 1.0f, 1.0f };
    e.size_over_life.keys_count = 2;
    e.size_over_life.keys[0] = { 0.0f, 1.0f };
    e.size_over_life.keys[1] = { 1.0f, 3.0f };
    e.alpha_over_life.keys_count = 2;
    e.alpha_over_life.keys[0] = { 0.0f, 1.0f };
    e.alpha_over_life.keys[1] = { 1.0f, 0.0f };
    ParticleSystem system(1);
    system.play();
    // half its life (the frame it spawned in doesn't age it)
    step(system, effect, 31);
    std::vector<ParticleInstance> out;
    system.gather(effect, {}, glm::vec4(0.5f, 1.0f, 1.0f, 1.0f), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_NEAR(out[0].size, 2.0f, 0.05f);
    EXPECT_NEAR(out[0].color.a, 0.5f, 0.03f);
    EXPECT_FLOAT_EQ(out[0].color.r, 0.5f);
}

TEST(ParticleSystem, StopLivesOutOrClears)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.looping = true;
    e.rate = 60.0f;
    e.lifetime = { 0.5f, 0.5f };
    ParticleSystem live_out(1);
    live_out.play();
    step(live_out, effect, 30);
    const auto before = live_out.get_count();
    ASSERT_GT(before, 0u);
    live_out.stop(engine::ParticleStopMode::eLiveOut);
    EXPECT_FALSE(live_out.is_playing());
    EXPECT_TRUE(live_out.is_alive());
    step(live_out, effect, 1);
    // no new ones, the old ones live on, then die
    EXPECT_LE(live_out.get_count(), before);
    EXPECT_GT(live_out.get_count(), 0u);
    step(live_out, effect, 40);
    EXPECT_FALSE(live_out.is_alive());

    ParticleSystem cleared(1);
    cleared.play();
    step(cleared, effect, 30);
    cleared.stop(engine::ParticleStopMode::eClear);
    EXPECT_EQ(cleared.get_count(), 0u);
    EXPECT_FALSE(cleared.is_alive());
}

TEST(ParticleSystem, ZeroDeltaFreezes)
{
    auto effect = quiet_effect();
    effect.emitters[0].rate = 60.0f;
    effect.emitters[0].speed = { 1.0f, 1.0f };
    ParticleSystem system(1);
    system.play();
    step(system, effect, 10);
    const auto before = gather(system, effect);
    system.update(effect, {}, 0.0f);
    const auto after = gather(system, effect);
    ASSERT_EQ(before.size(), after.size());
    EXPECT_EQ(before[0].position, after[0].position);
}

TEST(ParticleSystem, DisabledAndMissingEmittersEmitNothing)
{
    auto effect = quiet_effect();
    effect.emitters_count = 2;
    effect.emitters[0].bursts_count = 1;
    effect.emitters[0].bursts[0] = { 0.0f, 3 };
    effect.emitters[0].enabled = false;
    effect.emitters[1] = effect.emitters[0];
    effect.emitters[1].enabled = true;
    ParticleSystem system(1);
    system.play();
    step(system, effect, 1);
    EXPECT_EQ(system.get_count(), 3u);
    effect.emitters_count = 1;
    step(system, effect, 1);
    EXPECT_EQ(system.get_count(), 0u);
}

TEST(ParticleSort, BackToFrontThenSortOrder)
{
    std::vector<engine::ParticleSortKey> keys = { { 1.0f, 0, 0 }, { 5.0f, 0, 1 }, { 3.0f, 1, 2 }, { 3.0f, 0, 3 } };
    engine::sort_back_to_front(keys);
    EXPECT_EQ(keys[0].index, 1u);
    EXPECT_EQ(keys[1].index, 3u);
    EXPECT_EQ(keys[2].index, 2u);
    EXPECT_EQ(keys[3].index, 0u);
}
