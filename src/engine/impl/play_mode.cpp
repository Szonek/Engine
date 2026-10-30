#include "play_mode.h"
#include "logger.h"

#include <utility>

bool engine::PlayMode::enter(const std::function<PlaySnapshot()>& take_snapshot)
{
    if (state_ != ENGINE_PLAY_MODE_EDIT)
    {
        log::log(log::LogLevel::eError, "Play mode: Play while playing already.\n");
        return false;
    }
    snapshot_ = take_snapshot();
    state_ = ENGINE_PLAY_MODE_PLAYING;
    return true;
}

bool engine::PlayMode::set_paused(bool paused)
{
    if (state_ == ENGINE_PLAY_MODE_EDIT)
    {
        log::log(log::LogLevel::eError, "Play mode: there is nothing to pause or resume in Edit mode.\n");
        return false;
    }
    state_ = paused ? ENGINE_PLAY_MODE_PAUSED : ENGINE_PLAY_MODE_PLAYING;
    if (!paused)
    {
        step_requested_ = false;
    }
    return true;
}

bool engine::PlayMode::step()
{
    if (state_ != ENGINE_PLAY_MODE_PAUSED)
    {
        log::log(log::LogLevel::eError, "Play mode: Step only advances a paused game.\n");
        return false;
    }
    step_requested_ = true;
    return true;
}

bool engine::PlayMode::exit(const std::function<bool(const PlaySnapshot&)>& restore)
{
    if (!exit_allowed_)
    {
        log::log(log::LogLevel::eError, "Play mode: this application always plays (no editor, no play mode control).\n");
        return false;
    }
    if (state_ == ENGINE_PLAY_MODE_EDIT)
    {
        log::log(log::LogLevel::eError, "Play mode: Stop in Edit mode.\n");
        return false;
    }
    if (snapshot_ && !restore(*snapshot_))
    {
        log::log(log::LogLevel::eError, "Play mode: the scene saved at Play couldn't be loaded back; the play session goes on.\n");
        return false;
    }
    snapshot_.reset();
    state_ = ENGINE_PLAY_MODE_EDIT;
    step_requested_ = false;
    step_frame_ = false;
    session_count_++;
    return true;
}

void engine::PlayMode::begin_frame()
{
    step_frame_ = state_ == ENGINE_PLAY_MODE_PAUSED && std::exchange(step_requested_, false);
}
