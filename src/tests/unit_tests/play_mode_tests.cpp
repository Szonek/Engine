#include "play_mode.h"
#include "test_logger.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
using engine::PlayMode;
using engine::PlaySnapshot;

class PlayModeTest : public ::testing::Test
{
protected:
    void SetUp() override { test_logger::messages().clear(); }

    // Play saving `scene`
    bool enter(PlayMode& mode, const std::string& scene = "edit state", bool had_nav_mesh = false)
    {
        return mode.enter([&]() { return PlaySnapshot{ scene, had_nav_mesh }; });
    }

    // Stop, recording what it gives back
    bool exit(PlayMode& mode)
    {
        return mode.exit([this](const PlaySnapshot& snapshot) { restored_.push_back(snapshot); });
    }

    std::vector<PlaySnapshot> restored_;
};

TEST_F(PlayModeTest, ApplicationStartsPlaying)
{
    const PlayMode mode(true);
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PLAYING);
    EXPECT_TRUE(mode.is_playing());
    EXPECT_EQ(mode.get_session_count(), 0u);
}

TEST_F(PlayModeTest, WithoutPlayModeControlTheGameNeverStops)
{
    PlayMode mode(false);
    EXPECT_FALSE(exit(mode));
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PLAYING);
    EXPECT_TRUE(test_logger::logged("always plays"));
    // pausing is still the editor's and the tests' business, not refused
    EXPECT_TRUE(mode.set_paused(true));
}

TEST_F(PlayModeTest, StopOfTheFirstSessionLeavesTheSceneAsItIs)
{
    PlayMode mode(true);
    ASSERT_TRUE(exit(mode));
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_FALSE(mode.is_playing());
    // it had no snapshot: nothing to load back
    EXPECT_TRUE(restored_.empty());
    EXPECT_EQ(mode.get_session_count(), 1u);
}

TEST_F(PlayModeTest, StopGivesBackWhatPlaySaved)
{
    PlayMode mode(true);
    ASSERT_TRUE(exit(mode));
    ASSERT_TRUE(enter(mode, "level", true));
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PLAYING);
    ASSERT_TRUE(mode.set_paused(true));
    ASSERT_TRUE(exit(mode));
    ASSERT_EQ(restored_.size(), 1u);
    EXPECT_EQ(restored_[0].scene, "level");
    EXPECT_TRUE(restored_[0].had_nav_mesh);
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_EQ(mode.get_session_count(), 2u);

    // each session restores its own snapshot, once
    ASSERT_TRUE(enter(mode, "level after edits"));
    ASSERT_TRUE(exit(mode));
    ASSERT_EQ(restored_.size(), 2u);
    EXPECT_EQ(restored_[1].scene, "level after edits");
}

TEST_F(PlayModeTest, TransitionsWhichDontExistAreRefused)
{
    PlayMode mode(true);
    int snapshots = 0;
    // Play while playing keeps the session (and takes no snapshot)
    EXPECT_FALSE(mode.enter([&]() { snapshots++; return PlaySnapshot{}; }));
    EXPECT_EQ(snapshots, 0);
    // Step only while paused
    EXPECT_FALSE(mode.step());
    ASSERT_TRUE(exit(mode));
    // in Edit: no pause, no step, no second Stop
    EXPECT_FALSE(mode.set_paused(true));
    EXPECT_FALSE(mode.step());
    EXPECT_FALSE(exit(mode));
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_EQ(mode.get_session_count(), 1u);
}

TEST_F(PlayModeTest, PauseFreezesAndStepPlaysOneFrame)
{
    PlayMode mode(true);
    ASSERT_TRUE(mode.set_paused(true));
    mode.begin_frame();
    EXPECT_FALSE(mode.is_playing());

    ASSERT_TRUE(mode.step());
    // the step is taken by the next frame, and only that one
    EXPECT_FALSE(mode.is_playing());
    mode.begin_frame();
    EXPECT_TRUE(mode.is_playing());
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PAUSED);
    mode.begin_frame();
    EXPECT_FALSE(mode.is_playing());

    ASSERT_TRUE(mode.set_paused(false));
    mode.begin_frame();
    EXPECT_TRUE(mode.is_playing());
}

TEST_F(PlayModeTest, StepAskedForBeforeStopIsDropped)
{
    PlayMode mode(true);
    ASSERT_TRUE(mode.set_paused(true));
    ASSERT_TRUE(mode.step());
    ASSERT_TRUE(exit(mode));
    mode.begin_frame();
    EXPECT_FALSE(mode.is_playing());
}
}  // namespace
