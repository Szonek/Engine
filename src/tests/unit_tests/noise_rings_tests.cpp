#include "noise_rings.h"

#include <gtest/gtest.h>

using project_c::NoiseRings;

namespace
{
constexpr float DT = 1.0f / 60.0f;
constexpr float INTERVAL = 0.4f;

// frames in `seconds` of walking which pulse a new footstep ring
int footstep_pulses(NoiseRings& rings, float seconds, bool show = true)
{
    int pulses = 0;
    float last_remaining = 0.0f;
    for (int frame = 0; frame < static_cast<int>(seconds / DT + 0.5f); frame++)
    {
        rings.update(DT, 2.5f, false, glm::vec3(0.0f), show, INTERVAL);
        const auto& ring = rings.footstep_ring();
        if (ring && ring->remaining_seconds > last_remaining)
        {
            pulses++;
        }
        last_remaining = ring ? ring->remaining_seconds : 0.0f;
    }
    return pulses;
}
}  // namespace

TEST(NoiseRings, WalkingPulsesAFootstepRingEveryInterval)
{
    NoiseRings rings;
    // the first step at once, then every 0.4 s: 0, 0.4, 0.8 in 1 s
    EXPECT_EQ(footstep_pulses(rings, 1.0f), 3);
    ASSERT_TRUE(rings.footstep_ring());
    EXPECT_FLOAT_EQ(rings.footstep_ring()->radius, 2.5f);
    EXPECT_FALSE(rings.loud_ring());
}

TEST(NoiseRings, SilenceShowsNoRing)
{
    NoiseRings rings;
    for (int frame = 0; frame < 60; frame++)
    {
        rings.update(DT, 0.0f, false, glm::vec3(0.0f), true, INTERVAL);
        EXPECT_FALSE(rings.footstep_ring());
        EXPECT_FALSE(rings.loud_ring());
    }
}

TEST(NoiseRings, FootstepRingFadesAndTheNextStepAfterSilencePulsesAtOnce)
{
    NoiseRings rings;
    footstep_pulses(rings, 0.1f);
    ASSERT_TRUE(rings.footstep_ring());
    // sneaking: no new ring, the last one goes
    for (int frame = 0; frame < 30; frame++)
    {
        rings.update(DT, 0.0f, false, glm::vec3(0.0f), true, INTERVAL);
    }
    EXPECT_FALSE(rings.footstep_ring());
    rings.update(DT, 2.5f, false, glm::vec3(1.0f, 0.0f, 0.0f), true, INTERVAL);
    ASSERT_TRUE(rings.footstep_ring());
    EXPECT_FLOAT_EQ(rings.footstep_ring()->center.x, 1.0f);
}

TEST(NoiseRings, FootstepRingsCanBeHidden)
{
    NoiseRings rings;
    EXPECT_EQ(footstep_pulses(rings, 1.0f, false), 0);
    EXPECT_FALSE(rings.footstep_ring());
}

TEST(NoiseRings, LoudNoiseShowsItsOwnRing)
{
    NoiseRings rings;
    rings.update(DT, 5.0f, true, glm::vec3(0.0f, 0.0f, 2.0f), true, INTERVAL);
    ASSERT_TRUE(rings.loud_ring());
    EXPECT_FLOAT_EQ(rings.loud_ring()->radius, 5.0f);
    EXPECT_FLOAT_EQ(rings.loud_ring()->center.z, 2.0f);
    EXPECT_FALSE(rings.footstep_ring());
    // shown for LOUD_RING_SECONDS
    for (int frame = 0; frame < static_cast<int>(NoiseRings::LOUD_RING_SECONDS / DT) + 1; frame++)
    {
        rings.update(DT, 0.0f, false, glm::vec3(0.0f), true, INTERVAL);
    }
    EXPECT_FALSE(rings.loud_ring());
}
