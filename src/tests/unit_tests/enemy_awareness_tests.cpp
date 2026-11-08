#include "enemy_awareness.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace project_c;

namespace
{
constexpr float DT = 1.0f / 60.0f;

EnemyPerception perception()
{
    EnemyPerception p{};
    p.sight_range = 7.0f;
    p.sight_half_angle_degrees = 55.0f;
    p.close_range = 0.8f;
    p.eye_height = 0.6f;
    p.detect_seconds_near = 0.25f;
    p.detect_seconds_far = 1.5f;
    p.above_sight_multiplier = 0.35f;
    p.hearing_fill_seconds = 1.0f;
    p.loud_noise_fill = 0.6f;
    p.meter_decay_seconds = 3.0f;
    p.lose_seconds = 4.0f;
    p.suspicious_move_speed = 0.8f;
    p.lose_sight_grace_seconds = 0.5f;
    return p;
}

EnemyStimuli seen_at(float distance, bool above = false)
{
    EnemyStimuli s{};
    s.seen = true;
    s.seen_distance = distance;
    s.seen_above = above;
    s.seen_position = glm::vec3(0.0f, 0.0f, distance);
    return s;
}

// updates with `stimuli` for `seconds`; the seconds it took to reach `state`, or -1 when it didn't
float run_until(EnemyAwareness& awareness, const EnemyStimuli& stimuli, float seconds, Awareness state)
{
    for (int frame = 1; frame <= static_cast<int>(seconds / DT + 0.5f); frame++)
    {
        awareness.update(DT, stimuli, perception());
        if (awareness.state() == state)
        {
            return frame * DT;
        }
    }
    return -1.0f;
}

void run(EnemyAwareness& awareness, const EnemyStimuli& stimuli, float seconds)
{
    for (int frame = 0; frame < static_cast<int>(seconds / DT + 0.5f); frame++)
    {
        awareness.update(DT, stimuli, perception());
    }
}

EnemyAwareness alerted()
{
    EnemyAwareness awareness;
    EnemyStimuli hit{};
    hit.damaged = true;
    awareness.update(DT, hit, perception());
    return awareness;
}
}  // namespace

TEST(EnemyAwareness, SightConeInFront)
{
    const glm::vec3 forward(0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(in_sight_cone(glm::vec3(0.0f), forward, glm::vec3(0.0f, 0.0f, 5.0f), perception()));
    // within the half angle (45 of 55 degrees), and the height doesn't count
    EXPECT_TRUE(in_sight_cone(glm::vec3(0.0f), forward, glm::vec3(3.0f, 2.0f, 3.0f), perception()));
}

TEST(EnemyAwareness, SightConeOutsideTheAngleOrRange)
{
    const glm::vec3 forward(0.0f, 0.0f, 1.0f);
    // 60 degrees off the facing
    EXPECT_FALSE(in_sight_cone(glm::vec3(0.0f), forward, glm::vec3(std::sin(glm::radians(60.0f)), 0.0f, std::cos(glm::radians(60.0f))) * 3.0f, perception()));
    EXPECT_FALSE(in_sight_cone(glm::vec3(0.0f), forward, glm::vec3(0.0f, 0.0f, 7.5f), perception()));
}

TEST(EnemyAwareness, CloseRangeIsSeenFromBehind)
{
    const glm::vec3 forward(0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(in_sight_cone(glm::vec3(0.0f), forward, glm::vec3(0.0f, 0.0f, -0.7f), perception()));
    EXPECT_FALSE(in_sight_cone(glm::vec3(0.0f), forward, glm::vec3(0.0f, 0.0f, -1.0f), perception()));
}

TEST(EnemyAwareness, SightFillSecondsGoFromNearToFar)
{
    EXPECT_FLOAT_EQ(sight_fill_seconds(0.0f, perception(), false), 0.25f);
    EXPECT_FLOAT_EQ(sight_fill_seconds(7.0f, perception(), false), 1.5f);
    EXPECT_FLOAT_EQ(sight_fill_seconds(3.5f, perception(), false), 0.875f);
    // beyond the range it is the far end
    EXPECT_FLOAT_EQ(sight_fill_seconds(10.0f, perception(), false), 1.5f);
}

TEST(EnemyAwareness, PlayerAboveFillsTheMeterSlower)
{
    EXPECT_FLOAT_EQ(sight_fill_seconds(3.5f, perception(), true), 0.875f / 0.35f);
}

TEST(EnemyAwareness, SeenPlayerFillsTheMeterToAlertInTheExpectedTime)
{
    EnemyAwareness awareness;
    awareness.update(DT, seen_at(3.5f), perception());
    EXPECT_EQ(awareness.state(), Awareness::eSuspicious);
    EXPECT_GT(awareness.meter(), 0.0f);

    const float seconds = DT + run_until(awareness, seen_at(3.5f), 2.0f, Awareness::eAlert);
    EXPECT_NEAR(seconds, 0.875f, 2.0f * DT);
    EXPECT_FLOAT_EQ(awareness.meter(), 1.0f);
}

TEST(EnemyAwareness, MeterDecaysWhenNothingIsPerceived)
{
    EnemyAwareness awareness;
    run(awareness, seen_at(7.0f), 0.75f);
    ASSERT_EQ(awareness.state(), Awareness::eSuspicious);
    const float meter = awareness.meter();
    EXPECT_NEAR(meter, 0.5f, 0.02f);

    // half a meter empties in half the decay time
    const float seconds = run_until(awareness, {}, 3.0f, Awareness::eUnaware);
    EXPECT_NEAR(seconds, meter * 3.0f, 2.0f * DT);
    EXPECT_FLOAT_EQ(awareness.meter(), 0.0f);
}

TEST(EnemyAwareness, HeardFootstepsFillTheMeter)
{
    EnemyStimuli steps{};
    steps.heard_footsteps = true;
    steps.footsteps_position = glm::vec3(1.0f, 0.0f, 0.0f);
    EnemyAwareness awareness;
    EXPECT_NEAR(run_until(awareness, steps, 2.0f, Awareness::eAlert), 1.0f, 2.0f * DT);
}

TEST(EnemyAwareness, LoudNoiseFillsAtOnce)
{
    EnemyStimuli noise{};
    noise.heard_loud_noise = true;
    noise.loud_noise_position = glm::vec3(2.0f, 0.0f, 0.0f);
    EnemyAwareness awareness;
    awareness.update(DT, noise, perception());
    EXPECT_FLOAT_EQ(awareness.meter(), 0.6f);
    EXPECT_EQ(awareness.state(), Awareness::eSuspicious);
    EXPECT_EQ(awareness.last_known_position(), noise.loud_noise_position);

    // a second one fills it
    awareness.update(DT, noise, perception());
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
}

TEST(EnemyAwareness, DamageAlertsAtOnce)
{
    EnemyStimuli hit{};
    hit.damaged = true;
    hit.damage_position = glm::vec3(0.0f, 0.0f, -1.0f);
    EnemyAwareness awareness;
    awareness.update(DT, hit, perception());
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
    EXPECT_FLOAT_EQ(awareness.meter(), 1.0f);
    EXPECT_EQ(awareness.last_known_position(), hit.damage_position);
    EXPECT_TRUE(awareness.perceived_this_frame());
}

TEST(EnemyAwareness, LosingThePlayerSearchesThenGivesUp)
{
    auto awareness = alerted();
    // after the grace time without the player: searching
    EXPECT_NEAR(run_until(awareness, {}, 1.0f, Awareness::eSearching), 0.5f, 2.0f * DT);
    // for lose_seconds, then suspicious with a full meter
    EXPECT_NEAR(run_until(awareness, {}, 5.0f, Awareness::eSuspicious), 4.0f, 2.0f * DT);
    EXPECT_FLOAT_EQ(awareness.meter(), 1.0f);
    // which decays to unaware
    EXPECT_NEAR(run_until(awareness, {}, 4.0f, Awareness::eUnaware), 3.0f, 2.0f * DT);
}

// a frame or two without line of sight doesn't stop the chase (regression: the enemy flickered to searching)
TEST(EnemyAwareness, BrieflyLosingThePlayerKeepsHunting)
{
    auto awareness = alerted();
    EXPECT_TRUE(awareness.hunting(perception()));
    run(awareness, {}, 0.25f);
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
    EXPECT_TRUE(awareness.hunting(perception()));
    awareness.update(DT, seen_at(2.0f), perception());
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
    // lost for longer than the grace time: no longer hunting the player itself
    run(awareness, {}, 0.6f);
    EXPECT_FALSE(awareness.hunting(perception()));
    EXPECT_EQ(awareness.state(), Awareness::eSearching);
}

// alerted by its pack only, it never hunts the player itself: it goes to the last known position
TEST(EnemyAwareness, PackAlertAloneDoesNotHunt)
{
    EnemyStimuli pack{};
    pack.pack_alert = true;
    EnemyAwareness awareness;
    awareness.update(DT, pack, perception());
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
    EXPECT_FALSE(awareness.hunting(perception()));
}

TEST(EnemyAwareness, SeeingThePlayerWhileSearchingAlerts)
{
    auto awareness = alerted();
    run(awareness, {}, 1.0f);
    ASSERT_EQ(awareness.state(), Awareness::eSearching);
    awareness.update(DT, seen_at(6.0f), perception());
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
}

TEST(EnemyAwareness, PackAlertRaisesAnUnawareEnemy)
{
    EnemyStimuli pack{};
    pack.pack_alert = true;
    pack.pack_position = glm::vec3(4.0f, 0.0f, 4.0f);
    EnemyAwareness awareness;
    awareness.update(DT, pack, perception());
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
    EXPECT_EQ(awareness.last_known_position(), pack.pack_position);
    // the pack isn't its own perception
    EXPECT_FALSE(awareness.perceived_this_frame());

    // it stays alert while its pack is, and searches once the pack calms down
    run(awareness, pack, 2.0f);
    EXPECT_EQ(awareness.state(), Awareness::eAlert);
    awareness.update(DT, {}, perception());
    EXPECT_EQ(awareness.state(), Awareness::eSearching);
}

TEST(EnemyAwareness, LastKnownPositionFollowsTheNewestStimulus)
{
    EnemyAwareness awareness;
    EnemyStimuli steps{};
    steps.heard_footsteps = true;
    steps.footsteps_position = glm::vec3(1.0f, 0.0f, 0.0f);
    awareness.update(DT, steps, perception());
    EXPECT_EQ(awareness.last_known_position(), steps.footsteps_position);

    auto seen = seen_at(2.0f);
    seen.seen_position = glm::vec3(0.0f, 0.0f, 2.0f);
    awareness.update(DT, seen, perception());
    EXPECT_EQ(awareness.last_known_position(), seen.seen_position);

    // what it perceives itself is newer than what its pack says
    seen.pack_alert = true;
    seen.pack_position = glm::vec3(5.0f, 0.0f, 5.0f);
    awareness.update(DT, seen, perception());
    EXPECT_EQ(awareness.last_known_position(), seen.seen_position);

    // and nothing perceived keeps it
    awareness.update(DT, {}, perception());
    EXPECT_EQ(awareness.last_known_position(), seen.seen_position);
    EXPECT_GT(awareness.seconds_since_perceived(), 0.0f);
}

// a smoke cloud blinded it: an alert enemy searches where the player vanished and stops keeping its pack alert
TEST(EnemyAwareness, LoseTrackTurnsAlertIntoSearchingFromThePosition)
{
    const auto p = perception();
    project_c::EnemyAwareness awareness;
    project_c::EnemyStimuli hit{};
    hit.damaged = true;
    hit.damage_position = glm::vec3(1.0f, 0.0f, 1.0f);
    awareness.update(0.016f, hit, p);
    ASSERT_EQ(awareness.state(), project_c::Awareness::eAlert);
    ASSERT_EQ(awareness.seconds_since_perceived(), 0.0f);

    awareness.lose_track(glm::vec3(5.0f, 0.0f, 2.0f));
    EXPECT_EQ(awareness.state(), project_c::Awareness::eSearching);
    EXPECT_EQ(awareness.last_known_position(), glm::vec3(5.0f, 0.0f, 2.0f));
    EXPECT_FALSE(awareness.perceived_this_frame());
    EXPECT_TRUE(std::isinf(awareness.seconds_since_perceived()));
    EXPECT_FALSE(awareness.hunting(p));
    // it searches its full time from now on
    awareness.update(p.lose_seconds * 0.5f, project_c::EnemyStimuli{}, p);
    EXPECT_EQ(awareness.state(), project_c::Awareness::eSearching);
}

TEST(EnemyAwareness, LoseTrackLeavesAnUnawareEnemyAlone)
{
    project_c::EnemyAwareness awareness;
    awareness.lose_track(glm::vec3(5.0f, 0.0f, 2.0f));
    EXPECT_EQ(awareness.state(), project_c::Awareness::eUnaware);
    EXPECT_EQ(awareness.meter(), 0.0f);
}
