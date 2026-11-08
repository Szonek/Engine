#include "momentum.h"

#include <gtest/gtest.h>

using project_c::MomentumEvent;
using project_c::MomentumTuning;
using project_c::PlayerMomentum;

namespace
{
MomentumTuning tuning()
{
    return MomentumTuning{ 0.25f, 0.35f, 0.15f, 0.25f, 0.25f, 0.5f, 0.15f, 0.2f, 8.0f };
}
}  // namespace

TEST(Momentum, StartsEmpty)
{
    const PlayerMomentum momentum;
    EXPECT_EQ(momentum.value(), 0.0f);
    EXPECT_FALSE(momentum.is_draining());
    EXPECT_EQ(momentum.speed_multiplier(tuning()), 1.0f);
    EXPECT_EQ(momentum.chain_range_multiplier(tuning()), 1.0f);
}

TEST(Momentum, EachEventFillsItsAmount)
{
    const auto t = tuning();
    const std::pair<MomentumEvent, float> events[] = {
        { MomentumEvent::eGroundExecution, 0.25f },
        { MomentumEvent::ePlunge, 0.35f },
        { MomentumEvent::eChainLink, 0.15f },
        { MomentumEvent::eFallKill, 0.25f },
    };
    for (const auto& [event, amount] : events)
    {
        PlayerMomentum momentum;
        momentum.add(event, t);
        EXPECT_NEAR(momentum.value(), amount, 1e-6f);
    }
}

TEST(Momentum, FillsAddUpAndStopAtOne)
{
    const auto t = tuning();
    PlayerMomentum momentum;
    momentum.add(MomentumEvent::eGroundExecution, t);
    momentum.add(MomentumEvent::eChainLink, t);
    EXPECT_NEAR(momentum.value(), 0.4f, 1e-6f);
    for (int i = 0; i < 5; i++)
    {
        momentum.add(MomentumEvent::ePlunge, t);
    }
    EXPECT_EQ(momentum.value(), 1.0f);
}

TEST(Momentum, DrainsOnlyOnTheGroundInCombat)
{
    const auto t = tuning();
    PlayerMomentum momentum;
    momentum.add(MomentumEvent::ePlunge, t);
    momentum.add(MomentumEvent::ePlunge, t);
    // out of combat, or anywhere off the ground: it holds
    momentum.update(1.0f, false, t);
    EXPECT_NEAR(momentum.value(), 0.7f, 1e-6f);
    EXPECT_FALSE(momentum.is_draining());
    // on the ground in combat: 0.15 per second
    momentum.update(2.0f, true, t);
    EXPECT_NEAR(momentum.value(), 0.4f, 1e-5f);
    EXPECT_TRUE(momentum.is_draining());
    momentum.update(10.0f, true, t);
    EXPECT_EQ(momentum.value(), 0.0f);
    // empty, it no longer drains
    momentum.update(1.0f, true, t);
    EXPECT_FALSE(momentum.is_draining());
}

TEST(Momentum, HitDrainsAtOnce)
{
    const auto t = tuning();
    PlayerMomentum momentum;
    momentum.add(MomentumEvent::ePlunge, t);
    momentum.on_hit(t);
    EXPECT_NEAR(momentum.value(), 0.15f, 1e-6f);
    momentum.on_hit(t);
    EXPECT_EQ(momentum.value(), 0.0f);
}

TEST(Momentum, MultipliersScaleWithTheMeter)
{
    const auto t = tuning();
    PlayerMomentum momentum;
    momentum.add(MomentumEvent::eGroundExecution, t);
    momentum.add(MomentumEvent::eGroundExecution, t);
    EXPECT_NEAR(momentum.speed_multiplier(t), 1.125f, 1e-6f);
    EXPECT_NEAR(momentum.chain_range_multiplier(t), 1.25f, 1e-6f);
    for (int i = 0; i < 4; i++)
    {
        momentum.add(MomentumEvent::ePlunge, t);
    }
    EXPECT_NEAR(momentum.speed_multiplier(t), 1.25f, 1e-6f);
    EXPECT_NEAR(momentum.chain_range_multiplier(t), 1.5f, 1e-6f);
}
