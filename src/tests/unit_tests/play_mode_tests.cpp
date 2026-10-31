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

    // what the application does at the end of a frame
    bool finish_exit(PlayMode& mode)
    {
        return mode.finish_exit([this](const PlaySnapshot& snapshot) { restored_.push_back(snapshot); return true; });
    }

    // a whole Stop: asked for, the session's last frame, then finished at its end, recording what it gives back
    bool stop(PlayMode& mode)
    {
        if (!mode.exit())
        {
            return false;
        }
        mode.begin_frame();
        return mode.is_exit_due() && finish_exit(mode);
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
    EXPECT_FALSE(mode.exit());
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PLAYING);
    EXPECT_TRUE(test_logger::logged("always plays"));
    // pausing is still the editor's and the tests' business, not refused
    EXPECT_TRUE(mode.set_paused(true));
}

TEST_F(PlayModeTest, StopOfTheFirstSessionLeavesTheSceneAsItIs)
{
    PlayMode mode(true);
    ASSERT_TRUE(stop(mode));
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_FALSE(mode.is_playing());
    // it had no snapshot: nothing to load back
    EXPECT_TRUE(restored_.empty());
    EXPECT_EQ(mode.get_session_count(), 1u);
}

TEST_F(PlayModeTest, StopEndsTheSessionInAFrameOfItsOwnBeforeLoadingTheEditStateBack)
{
    PlayMode mode(true);
    ASSERT_TRUE(stop(mode));
    ASSERT_TRUE(enter(mode, "level"));
    mode.begin_frame();

    ASSERT_TRUE(mode.exit());
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EXITING);
    // nothing plays from now on, and nothing is loaded back before the session's last frame
    EXPECT_FALSE(mode.is_playing());
    EXPECT_FALSE(mode.is_exit_due());
    EXPECT_TRUE(restored_.empty());
    EXPECT_EQ(mode.get_session_count(), 1u);

    // the last frame: the edit state comes back at its end
    mode.begin_frame();
    EXPECT_FALSE(mode.is_playing());
    ASSERT_TRUE(mode.is_exit_due());
    ASSERT_TRUE(finish_exit(mode));
    ASSERT_EQ(restored_.size(), 1u);
    EXPECT_EQ(restored_[0].scene, "level");
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_FALSE(mode.is_exit_due());
    EXPECT_EQ(mode.get_session_count(), 2u);
}

TEST_F(PlayModeTest, NothingElseHappensWhileStopping)
{
    PlayMode mode(true);
    ASSERT_TRUE(mode.set_paused(true));
    ASSERT_TRUE(mode.exit());
    int snapshots = 0;
    EXPECT_FALSE(mode.enter([&]() { snapshots++; return PlaySnapshot{}; }));
    EXPECT_EQ(snapshots, 0);
    EXPECT_FALSE(mode.set_paused(false));
    EXPECT_TRUE(test_logger::logged("stopping"));
    EXPECT_FALSE(mode.step());
    EXPECT_FALSE(mode.exit());
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EXITING);
}

TEST_F(PlayModeTest, StopGivesBackWhatPlaySaved)
{
    PlayMode mode(true);
    ASSERT_TRUE(stop(mode));
    ASSERT_TRUE(enter(mode, "level", true));
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PLAYING);
    ASSERT_TRUE(mode.set_paused(true));
    ASSERT_TRUE(stop(mode));
    ASSERT_EQ(restored_.size(), 1u);
    EXPECT_EQ(restored_[0].scene, "level");
    EXPECT_TRUE(restored_[0].had_nav_mesh);
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_EQ(mode.get_session_count(), 2u);

    // each session restores its own snapshot, once
    ASSERT_TRUE(enter(mode, "level after edits"));
    ASSERT_TRUE(stop(mode));
    ASSERT_EQ(restored_.size(), 2u);
    EXPECT_EQ(restored_[1].scene, "level after edits");
}

TEST_F(PlayModeTest, StopWhoseSnapshotCantBeLoadedKeepsThePlaySession)
{
    PlayMode mode(true);
    ASSERT_TRUE(stop(mode));
    ASSERT_TRUE(enter(mode, "level"));
    ASSERT_TRUE(mode.set_paused(true));
    const auto sessions = mode.get_session_count();
    ASSERT_TRUE(mode.exit());
    mode.begin_frame();
    EXPECT_FALSE(mode.finish_exit([](const PlaySnapshot&) { return false; }));
    EXPECT_TRUE(test_logger::logged("couldn't be loaded back"));
    // as it was before Stop
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_PAUSED);
    EXPECT_FALSE(mode.is_exit_due());
    EXPECT_EQ(mode.get_session_count(), sessions);

    // the snapshot is kept for the next Stop
    ASSERT_TRUE(stop(mode));
    ASSERT_EQ(restored_.size(), 1u);
    EXPECT_EQ(restored_[0].scene, "level");
    EXPECT_EQ(mode.get_state(), ENGINE_PLAY_MODE_EDIT);
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
    // nothing to finish without a Stop
    EXPECT_FALSE(finish_exit(mode));
    ASSERT_TRUE(stop(mode));
    // in Edit: no pause, no step, no second Stop
    EXPECT_FALSE(mode.set_paused(true));
    EXPECT_FALSE(mode.step());
    EXPECT_FALSE(mode.exit());
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
    ASSERT_TRUE(mode.exit());
    // the session's last frame doesn't play
    mode.begin_frame();
    EXPECT_FALSE(mode.is_playing());
    ASSERT_TRUE(finish_exit(mode));
    mode.begin_frame();
    EXPECT_FALSE(mode.is_playing());
}
}  // namespace
