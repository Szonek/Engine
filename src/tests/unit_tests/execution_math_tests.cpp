#include "execution_math.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace project_c;
using namespace project_c::execution_math;

namespace
{
constexpr float GRAVITY = 10.0f;

// the ground execution's: 1.4 m, 0.5 m up or down, the 140 degree arc at the back
Rules ground_rules()
{
    Rules rules{};
    rules.range = 1.4f;
    rules.max_height_difference = 0.5f;
    rules.min_angle_from_facing = 180.0f - 70.0f;
    return rules;
}

Candidate behind(float distance)
{
    Candidate candidate{};
    candidate.distance = distance;
    candidate.angle_from_facing = 180.0f;
    return candidate;
}

glm::vec3 at_angle(float degrees, float distance)
{
    // around +y from the facing (+z)
    return glm::vec3(std::sin(glm::radians(degrees)), 0.0f, std::cos(glm::radians(degrees))) * distance;
}
}  // namespace

TEST(ExecutionMath, AngleFromFacing)
{
    const glm::vec3 forward(0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(angle_from_facing(glm::vec3(0.0f), forward, at_angle(0.0f, 1.0f)), 0.0f, 0.01f);
    EXPECT_NEAR(angle_from_facing(glm::vec3(0.0f), forward, at_angle(180.0f, 1.0f)), 180.0f, 0.01f);
    EXPECT_NEAR(angle_from_facing(glm::vec3(0.0f), forward, at_angle(110.0f, 1.0f)), 110.0f, 0.01f);
    EXPECT_NEAR(angle_from_facing(glm::vec3(0.0f), forward, at_angle(-90.0f, 2.0f)), 90.0f, 0.01f);
    // the height doesn't count
    EXPECT_NEAR(angle_from_facing(glm::vec3(0.0f), forward, at_angle(180.0f, 1.0f) + glm::vec3(0.0f, 3.0f, 0.0f)), 180.0f, 0.01f);
}

TEST(ExecutionMath, OnlyFromTheArcAtTheBack)
{
    auto candidate = behind(1.0f);
    EXPECT_TRUE(executable(candidate, ground_rules()));
    // 70 degrees off the back direction: the arc's edge
    candidate.angle_from_facing = 110.0f;
    EXPECT_TRUE(executable(candidate, ground_rules()));
    candidate.angle_from_facing = 100.0f;
    EXPECT_FALSE(executable(candidate, ground_rules()));
    // from the side
    candidate.angle_from_facing = 90.0f;
    EXPECT_FALSE(executable(candidate, ground_rules()));
}

TEST(ExecutionMath, OnlyWhileNotNoticed)
{
    auto candidate = behind(1.0f);
    candidate.awareness = Awareness::eSuspicious;
    EXPECT_TRUE(executable(candidate, ground_rules()));
    // a suspicious enemy looking at the player isn't
    candidate.sees_player = true;
    EXPECT_FALSE(executable(candidate, ground_rules()));
    candidate.sees_player = false;
    for (const auto aware : { Awareness::eAlert, Awareness::eSearching })
    {
        candidate.awareness = aware;
        EXPECT_FALSE(executable(candidate, ground_rules()));
    }
}

TEST(ExecutionMath, RangeHeightReachAndAvailability)
{
    EXPECT_TRUE(executable(behind(1.4f), ground_rules()));
    EXPECT_FALSE(executable(behind(1.5f), ground_rules()));
    auto candidate = behind(1.0f);
    candidate.height_difference = 0.6f;
    EXPECT_FALSE(executable(candidate, ground_rules()));
    candidate.height_difference = -0.5f;
    EXPECT_TRUE(executable(candidate, ground_rules()));
    candidate.reachable = false;
    EXPECT_FALSE(executable(candidate, ground_rules()));
    candidate.reachable = true;
    candidate.unavailable = true;
    EXPECT_FALSE(executable(candidate, ground_rules()));
}

TEST(ExecutionMath, KnockedDownIsExecutableFromAnySideWhateverItKnows)
{
    Candidate candidate{};
    candidate.any_side = true;
    candidate.awareness = Awareness::eAlert;
    candidate.sees_player = true;
    candidate.distance = 1.0f;
    candidate.angle_from_facing = 0.0f;
    EXPECT_TRUE(executable(candidate, ground_rules()));
    // not as a plunge's target
    auto plunge = ground_rules();
    plunge.allow_any_side = false;
    EXPECT_FALSE(executable(candidate, plunge));
}

TEST(ExecutionMath, PlungeTakesAnyAwareness)
{
    Rules plunge{};
    plunge.range = 2.5f;
    plunge.max_height_difference = std::numeric_limits<float>::infinity();
    plunge.any_awareness = true;
    plunge.allow_any_side = false;
    Candidate candidate{};
    candidate.awareness = Awareness::eAlert;
    candidate.sees_player = true;
    candidate.distance = 2.0f;
    candidate.height_difference = 3.0f;
    EXPECT_TRUE(executable(candidate, plunge));
    candidate.distance = 2.6f;
    EXPECT_FALSE(executable(candidate, plunge));
}

TEST(ExecutionMath, PicksTheNearestThenTheMostBehind)
{
    std::vector<Candidate> candidates = { behind(1.1f), behind(0.8f), behind(0.5f) };
    // the nearest can't be executed: it sees the player
    candidates[2].sees_player = true;
    EXPECT_EQ(pick_target(candidates, ground_rules()), 1u);

    std::vector<Candidate> tied = { behind(0.8f), behind(0.8f) };
    tied[0].angle_from_facing = 130.0f;
    EXPECT_EQ(pick_target(tied, ground_rules()), 1u);

    std::vector<Candidate> none = { behind(2.0f) };
    EXPECT_FALSE(pick_target(none, ground_rules()).has_value());
}

TEST(ExecutionMath, ExecutionSpotIsBehindTheVictim)
{
    const auto spot = execution_spot(glm::vec3(2.0f, 1.0f, 3.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.6f);
    EXPECT_FLOAT_EQ(spot.x, 2.0f - 0.6f);
    EXPECT_FLOAT_EQ(spot.y, 1.0f);
    EXPECT_FLOAT_EQ(spot.z, 3.0f);
}

TEST(ExecutionMath, PredictsWhereAFallLands)
{
    // thrown up at 3 m/s and forward at 2 m/s from 2.25 m, down to 0
    const glm::vec3 start(0.0f, 2.25f, 0.0f);
    const glm::vec3 velocity(2.0f, 3.0f, 0.0f);
    const auto landing = predict_landing(start, velocity, GRAVITY, 0.0f);
    ASSERT_TRUE(landing.has_value());
    const float t = landing->seconds;
    EXPECT_NEAR(start.y + velocity.y * t - 0.5f * GRAVITY * t * t, 0.0f, 1e-4f);
    EXPECT_NEAR(landing->point.x, velocity.x * t, 1e-4f);
    EXPECT_FLOAT_EQ(landing->point.y, 0.0f);
    // below the height and falling: never
    EXPECT_FALSE(predict_landing(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), GRAVITY, 1.0f).has_value());
}

TEST(ExecutionMath, MoveSecondsHaveAMinimum)
{
    EXPECT_FLOAT_EQ(move_seconds(2.8f, 14.0f, 0.12f), 0.2f);
    EXPECT_FLOAT_EQ(move_seconds(0.5f, 14.0f, 0.12f), 0.12f);
}

TEST(ExecutionMath, KnockdownRadiusGrowsWithTheDropUpToItsMax)
{
    EXPECT_FLOAT_EQ(knockdown_radius(2.0f, 0.6f, 3.0f), 1.2f);
    EXPECT_FLOAT_EQ(knockdown_radius(10.0f, 0.6f, 3.0f), 3.0f);
    EXPECT_FLOAT_EQ(knockdown_radius(-1.0f, 0.6f, 3.0f), 0.0f);
}

TEST(ExecutionMath, ChainWindowOpensAndExpires)
{
    ChainWindow window;
    EXPECT_FALSE(window.is_open());
    window.open(1.0f);
    window.update(0.6f);
    EXPECT_TRUE(window.is_open());
    window.update(0.5f);
    EXPECT_FALSE(window.is_open());
    window.open(1.0f);
    window.close();
    EXPECT_FALSE(window.is_open());
}
