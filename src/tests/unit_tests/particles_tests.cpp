#include "particles.h"
#include "serialization/builtin_types.h"
#include "serialization/game_types.h"
#include "serialization/scene_serializer.h"
#include "serialization/type_registry.h"
#include "test_logger.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The particle simulation (impl/particles.h): emission timing, bursts, rate, capacity, lifetimes, curves, determinism,
// world and local space, stopping and death; and the effect as a data asset.
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

// ---- the effect as a data asset (the engine's type "particle_effect") ----
namespace
{
// resolves nothing: the effects in these tests reference no asset
class NoAssets : public engine::IAssetResolver
{
public:
    std::string get_asset_id(engine::AssetKind, std::uint32_t) const override { return {}; }
    std::uint32_t resolve(engine::AssetKind, std::string_view) override { return ENGINE_INVALID_OBJECT_HANDLE; }
    const engine::TypeInfo* get_data_asset_type(std::uint32_t) const override { return nullptr; }
};
}  // namespace

TEST(ParticleEffectAsset, IsAnEngineDataAssetType)
{
    engine::TypeRegistry types;
    engine::register_builtin_types(types);
    const auto* type = types.find(engine::PARTICLE_EFFECT_TYPE_NAME);
    ASSERT_NE(type, nullptr);
    EXPECT_TRUE(type->data_asset_type);
    EXPECT_FALSE(type->game_defined);
    EXPECT_FALSE(type->is_component());
    EXPECT_EQ(type->size, sizeof(ParticleEffectDesc));
}

TEST(ParticleEffectAsset, FileLoadsOverTheDefaultsAndRoundTrips)
{
    engine::TypeRegistry types;
    engine::register_builtin_types(types);
    NoAssets assets;
    const auto loaded = engine::load_data_asset(R"({ "version": 1, "type": "particle_effect", "value": {
        "emitters_count": 2,
        "emitters": [ { "rate": 5, "shape": "sphere", "bursts_count": 1, "bursts": [ { "time": 0.25, "count": 12 } ] }, { "blend": "additive" } ] } })",
        "effects/test.json", types, assets);
    ASSERT_TRUE(loaded);
    const auto& effect = *reinterpret_cast<const ParticleEffectDesc*>(loaded->value->get());
    EXPECT_EQ(effect.emitters_count, 2u);
    EXPECT_EQ(effect.emitters[0].rate, 5.0f);
    EXPECT_EQ(effect.emitters[0].shape, engine::ParticleSpawnShape::eSphere);
    EXPECT_EQ(effect.emitters[0].bursts[0].count, 12u);
    EXPECT_EQ(effect.emitters[1].blend, engine::ParticleBlendMode::eAdditive);
    // what the file doesn't say keeps the default effect's values
    EXPECT_EQ(effect.emitters[0].max_particles, engine::default_particle_emitter().max_particles);
    EXPECT_EQ(effect.emitters[1].rate, engine::default_particle_emitter().rate);

    const auto text = engine::save_data_asset(*loaded->type, loaded->value->get(), assets);
    const auto again = engine::load_data_asset(text, "effects/test.json", types, assets);
    ASSERT_TRUE(again);
    EXPECT_TRUE(engine::equal_values(*loaded->type, loaded->value->get(), again->value->get()));
}

TEST(ParticleEffectAsset, GameFieldKindReferencesIt)
{
    engine::TypeRegistry types;
    engine::register_builtin_types(types);
    struct Torch
    {
        engine_data_asset_t flame;
    };
    engine_field_desc_t field{};
    field.name = "flame";
    field.kind = ENGINE_FIELD_KIND_PARTICLE_EFFECT;
    field.offset = 0;
    engine_type_desc_t desc{};
    desc.name = "torch";
    desc.size = sizeof(Torch);
    desc.alignment = alignof(Torch);
    desc.fields = &field;
    desc.fields_count = 1;
    const auto* type = engine::register_game_type(types, desc, true);
    ASSERT_NE(type, nullptr);
    ASSERT_EQ(type->fields.size(), 1u);
    EXPECT_EQ(type->fields[0].kind, engine::FieldKind::eAsset);
    EXPECT_EQ(type->fields[0].asset_kind, engine::AssetKind::eDataAsset);
    EXPECT_EQ(type->fields[0].data_type, types.find(engine::PARTICLE_EFFECT_TYPE_NAME));
}

TEST(ParticleEffectAsset, ShortListsOfStructsAreReadWithoutComplaint)
{
    engine::TypeRegistry types;
    engine::register_builtin_types(types);
    NoAssets assets;
    test_logger::messages().clear();
    const auto loaded = engine::load_data_asset(R"({ "version": 1, "type": "particle_effect", "value": {
        "emitters_count": 1, "emitters": [ { "bursts_count": 1, "bursts": [ { "time": 0, "count": 3 } ] } ] } })", "effects/short.json", types, assets);
    ASSERT_TRUE(loaded);
    EXPECT_TRUE(test_logger::messages().empty());
    const auto& effect = *reinterpret_cast<const ParticleEffectDesc*>(loaded->value->get());
    EXPECT_EQ(effect.emitters[0].bursts[0].count, 3u);
    // the elements left out keep the default effect's values
    EXPECT_EQ(effect.emitters[1].rate, engine::default_particle_emitter().rate);

    // a longer list loses what doesn't fit, and says so
    std::string bursts;
    for (int i = 0; i < 9; i++)
    {
        bursts += std::string(i ? ", " : "") + "{ \"time\": 0, \"count\": 1 }";
    }
    ASSERT_TRUE(engine::load_data_asset(R"({ "version": 1, "type": "particle_effect", "value": { "emitters": [ { "bursts": [ )" + bursts + " ] } ] } }",
        "effects/long.json", types, assets));
    EXPECT_TRUE(test_logger::logged("has 9 elements instead of 8"));
}

namespace
{
ParticleEmitterDesc flipbook_emitter(std::uint32_t columns, std::uint32_t rows, float fps, bool over_life)
{
    auto e = engine::default_particle_emitter();
    e.flipbook_columns = columns;
    e.flipbook_rows = rows;
    e.flipbook_fps = fps;
    e.flipbook_over_life = over_life;
    return e;
}
}  // namespace

TEST(ParticleErosion, NoKeysIsNoErosion)
{
    const auto e = engine::default_particle_emitter();
    EXPECT_EQ(engine::particle_erosion(e, 0.0f), 0.0f);
    EXPECT_EQ(engine::particle_erosion(e, 1.0f), 0.0f);
}

TEST(ParticleErosion, FollowsItsCurveClampedToZeroToOne)
{
    auto e = engine::default_particle_emitter();
    e.erosion_over_life.keys_count = 3;
    e.erosion_over_life.keys[0] = { 0.0f, -0.5f };
    e.erosion_over_life.keys[1] = { 0.5f, 0.25f };
    e.erosion_over_life.keys[2] = { 1.0f, 2.0f };
    EXPECT_EQ(engine::particle_erosion(e, 0.0f), 0.0f);
    EXPECT_NEAR(engine::particle_erosion(e, 0.5f), 0.25f, 1e-6f);
    EXPECT_EQ(engine::particle_erosion(e, 1.0f), 1.0f);
}

TEST(ParticleFlipbook, OneFrameIsAlwaysFrameZero)
{
    for (const bool over_life : { false, true })
    {
        const auto f = engine::particle_flipbook_frame(flipbook_emitter(1, 1, 30.0f, over_life), 0.7f, 1.0f);
        EXPECT_EQ(f.frame, 0u);
        EXPECT_EQ(f.next_frame, 0u);
        EXPECT_EQ(f.blend, 0.0f);
    }
    // a zero sized grid counts as one frame
    const auto f = engine::particle_flipbook_frame(flipbook_emitter(0, 0, 30.0f, false), 0.7f, 1.0f);
    EXPECT_EQ(f.frame, 0u);
    EXPECT_EQ(f.blend, 0.0f);
}

TEST(ParticleFlipbook, FpsPlaysFramesFromBirthAndLoops)
{
    // 2 x 2 frames at 10 fps: a frame every 0.1 s, the four again from 0.4 s
    const auto e = flipbook_emitter(2, 2, 10.0f, false);
    auto f = engine::particle_flipbook_frame(e, 0.0f, 5.0f);
    EXPECT_EQ(f.frame, 0u);
    EXPECT_EQ(f.blend, 0.0f);

    f = engine::particle_flipbook_frame(e, 0.125f, 5.0f);
    EXPECT_EQ(f.frame, 1u);
    EXPECT_EQ(f.next_frame, 2u);
    EXPECT_NEAR(f.blend, 0.25f, 1e-4f);

    // the last frame blends into the first
    f = engine::particle_flipbook_frame(e, 0.35f, 5.0f);
    EXPECT_EQ(f.frame, 3u);
    EXPECT_EQ(f.next_frame, 0u);
    EXPECT_NEAR(f.blend, 0.5f, 1e-4f);

    // and it loops, whatever the lifetime
    f = engine::particle_flipbook_frame(e, 0.525f, 5.0f);
    EXPECT_EQ(f.frame, 1u);
    EXPECT_NEAR(f.blend, 0.25f, 1e-3f);
}

TEST(ParticleFlipbook, ZeroFpsHoldsTheFirstFrame)
{
    const auto f = engine::particle_flipbook_frame(flipbook_emitter(4, 4, 0.0f, false), 3.0f, 5.0f);
    EXPECT_EQ(f.frame, 0u);
    EXPECT_EQ(f.blend, 0.0f);
}

TEST(ParticleFlipbook, OverLifePlaysOnceAndHoldsTheLastFrame)
{
    // 4 x 1 frames over a 2 s life: frame i at i / 3 of it; the fps doesn't matter
    const auto e = flipbook_emitter(4, 1, 100.0f, true);
    auto f = engine::particle_flipbook_frame(e, 0.0f, 2.0f);
    EXPECT_EQ(f.frame, 0u);
    EXPECT_EQ(f.next_frame, 1u);
    EXPECT_EQ(f.blend, 0.0f);

    // halfway: between frames 1 and 2
    f = engine::particle_flipbook_frame(e, 1.0f, 2.0f);
    EXPECT_EQ(f.frame, 1u);
    EXPECT_EQ(f.next_frame, 2u);
    EXPECT_NEAR(f.blend, 0.5f, 1e-4f);

    // at and past the end: the last frame alone
    for (const float age : { 2.0f, 3.0f })
    {
        f = engine::particle_flipbook_frame(e, age, 2.0f);
        EXPECT_EQ(f.frame, 3u);
        EXPECT_EQ(f.next_frame, 3u);
        EXPECT_EQ(f.blend, 0.0f);
    }
}

TEST(ParticleFlipbook, GatheredParticlesCarryTheirFrameAndErosion)
{
    auto effect = quiet_effect();
    auto& e = effect.emitters[0];
    e.bursts_count = 1;
    e.bursts[0] = { 0.0f, 1 };
    e.lifetime = { 1.0f, 1.0f };
    e.flipbook_columns = 2;
    e.flipbook_rows = 1;
    e.flipbook_over_life = true;
    e.erosion_over_life.keys_count = 2;
    e.erosion_over_life.keys[0] = { 0.0f, 0.0f };
    e.erosion_over_life.keys[1] = { 1.0f, 1.0f };
    ParticleSystem system(1);
    system.play();
    // 30 frames: half the life
    step(system, effect, 30);
    const auto particles = gather(system, effect);
    ASSERT_EQ(particles.size(), 1u);
    EXPECT_NEAR(particles[0].erosion, 0.5f, 0.02f);
    EXPECT_EQ(particles[0].flipbook.frame, 0u);
    EXPECT_EQ(particles[0].flipbook.next_frame, 1u);
    EXPECT_NEAR(particles[0].flipbook.blend, 0.5f, 0.02f);
}
