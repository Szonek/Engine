#pragma once
#include <engine.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace engine
{
// What Play saved of the scene, which Stop loads back.
struct PlaySnapshot
{
    // the active scene as a scene file (Scene::save)
    std::string scene;
    // whether it had a built nav mesh: derived data, not in the file, so Stop builds it again
    bool had_nav_mesh = false;
};

// The play state of an application (Edit, Playing, Paused) and the snapshot taken at Play, like Unity's
// EditorApplication.EnterPlaymode / isPaused / Step / ExitPlaymode: Play saves the edit state, Stop throws the play
// session away and loads that back (Unity's snapshot-and-restore model). The application starts playing, a session
// without a snapshot; leaving play mode (exit) is only allowed when the application lets something control it (the
// editor, tests), the shipped game never stops playing. GL-free: the caller takes and restores the snapshot.
class PlayMode
{
public:
    explicit PlayMode(bool exit_allowed) : exit_allowed_(exit_allowed) {}

    engine_play_mode_state_t get_state() const { return state_; }
    // whether the game plays this frame (scripts tick): playing, or paused with a step taken for this frame
    bool is_playing() const { return state_ == ENGINE_PLAY_MODE_PLAYING || step_frame_; }
    // the number of play sessions which ended (exit), so a caller notices a Stop without a callback
    std::uint32_t get_session_count() const { return session_count_; }

    // Edit -> Playing, keeping what `take_snapshot` saves; false (logged) when not in Edit
    bool enter(const std::function<PlaySnapshot()>& take_snapshot);
    // Playing <-> Paused; false (logged) in Edit
    bool set_paused(bool paused);
    // while paused, the next frame plays (begin_frame); false (logged) when not paused
    bool step();
    // Playing or Paused -> Edit: `restore` gets what was saved at Play (it isn't called for the session the
    // application started in, which has no snapshot: the scene stays as it is), then the session count moves. False
    // (logged) in Edit or when exits aren't allowed; false when `restore` fails (returns false), then the session goes
    // on and keeps its snapshot, so nothing of the edit state is lost and Stop can be tried again.
    bool exit(const std::function<bool(const PlaySnapshot&)>& restore);

    // at the start of every frame: a step asked for plays this frame
    void begin_frame();

private:
    const bool exit_allowed_;
    engine_play_mode_state_t state_ = ENGINE_PLAY_MODE_PLAYING;
    std::optional<PlaySnapshot> snapshot_;
    std::uint32_t session_count_ = 0;
    bool step_requested_ = false;
    bool step_frame_ = false;
};
}  // namespace engine
