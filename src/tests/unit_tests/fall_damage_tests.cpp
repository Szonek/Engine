#include "fall_damage.h"

#include <gtest/gtest.h>

using project_c::FallDamage;
using project_c::FallMeasure;
using project_c::fall_damage_fraction;

namespace
{
// the skeleton's curve
constexpr FallDamage SKELETON{ 1.0f, 2.0f };
}  // namespace

TEST(FallDamage, NoDamageUpToTheMinHeight)
{
    EXPECT_EQ(fall_damage_fraction(SKELETON, 0.0f, false), 0.0f);
    EXPECT_EQ(fall_damage_fraction(SKELETON, 0.5f, false), 0.0f);
    EXPECT_EQ(fall_damage_fraction(SKELETON, 1.0f, false), 0.0f);
    EXPECT_EQ(fall_damage_fraction(SKELETON, -3.0f, false), 0.0f);
}

TEST(FallDamage, LinearBetweenTheMinAndTheLethalHeight)
{
    EXPECT_NEAR(fall_damage_fraction(SKELETON, 1.25f, false), 0.25f, 1e-5f);
    EXPECT_NEAR(fall_damage_fraction(SKELETON, 1.5f, false), 0.5f, 1e-5f);
    EXPECT_NEAR(fall_damage_fraction(SKELETON, 1.75f, false), 0.75f, 1e-5f);
    // the player's curve: half the HP from 5.5 m
    EXPECT_NEAR(fall_damage_fraction(FallDamage{ 3.0f, 8.0f }, 5.5f, false), 0.5f, 1e-5f);
}

TEST(FallDamage, LethalAtAndAboveTheLethalHeight)
{
    EXPECT_EQ(fall_damage_fraction(SKELETON, 2.0f, false), 1.0f);
    // the house roof
    EXPECT_EQ(fall_damage_fraction(SKELETON, 2.25f, false), 1.0f);
    EXPECT_EQ(fall_damage_fraction(SKELETON, 50.0f, false), 1.0f);
}

TEST(FallDamage, RollHalvesIt)
{
    EXPECT_NEAR(fall_damage_fraction(SKELETON, 1.5f, true), 0.25f, 1e-5f);
    EXPECT_NEAR(fall_damage_fraction(SKELETON, 3.0f, true), 0.5f, 1e-5f);
    EXPECT_EQ(fall_damage_fraction(SKELETON, 0.5f, true), 0.0f);
}

TEST(FallDamage, CurveWithoutARangeKillsFromItsMinHeight)
{
    const FallDamage cliff{ 2.0f, 2.0f };
    EXPECT_EQ(fall_damage_fraction(cliff, 2.0f, false), 0.0f);
    EXPECT_EQ(fall_damage_fraction(cliff, 2.01f, false), 1.0f);
}

TEST(FallMeasure, DropIsFromTheStartToTheLanding)
{
    FallMeasure fall;
    EXPECT_FALSE(fall.is_falling());
    fall.start(2.25f);
    EXPECT_TRUE(fall.is_falling());
    fall.update(1.0f);
    EXPECT_NEAR(fall.land(0.0f), 2.25f, 1e-5f);
    EXPECT_FALSE(fall.is_falling());
}

// a leap (or a knockback off a ledge which lifts first) counts from the top of its arc
TEST(FallMeasure, LeapCountsFromItsTop)
{
    FallMeasure fall;
    fall.start(3.0f);
    fall.update(3.6f);
    fall.update(4.0f);
    fall.update(2.0f);
    EXPECT_NEAR(fall.land(0.0f), 4.0f, 1e-5f);
}

TEST(FallMeasure, LandingHigherThanTheStartIsNoDrop)
{
    FallMeasure fall;
    fall.start(1.0f);
    EXPECT_EQ(fall.land(1.5f), 0.0f);
}
