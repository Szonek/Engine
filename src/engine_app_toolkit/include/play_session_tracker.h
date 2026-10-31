#pragma once
#include "engine.h"

#include <cstdint>

namespace engine
{
// Playing or Paused: a play session is under way (in Edit, and in the last frame of a session a Stop ends, none is)
inline bool is_play_session_state()
{
    const auto state = enginePlayModeGetState();
    return state == ENGINE_PLAY_MODE_PLAYING || state == ENGINE_PLAY_MODE_PAUSED;
}

// Follows the engine's play sessions from one update to the next, for the application and each scene: a session ends
// in the frame a Stop ends it in (Exiting: the scene is still there), or when the session count moved without that frame
// being seen (a scene not updated then), and begins when the game plays or is paused (Play, or the first update of an
// application which starts playing; also a session going on because the Stop couldn't load the edit state back). A
// Stop and a Play between two updates end the old session and begin the new one in the same update.
class PlaySessionTracker
{
public:
    struct Changes
    {
        // handled first: the old session goes before a new one begins
        bool ended = false;
        bool began = false;
    };

    // a Stop before the tracker existed isn't its session's
    PlaySessionTracker() : seen_sessions_(enginePlayModeGetSessionCount()) {}

    Changes update()
    {
        Changes changes;
        const auto sessions = enginePlayModeGetSessionCount();
        if (in_session_ && (sessions != seen_sessions_ || enginePlayModeGetState() == ENGINE_PLAY_MODE_EXITING))
        {
            changes.ended = true;
            in_session_ = false;
        }
        seen_sessions_ = sessions;
        if (!in_session_ && is_play_session_state())
        {
            in_session_ = true;
            changes.began = true;
        }
        return changes;
    }

    bool in_session() const { return in_session_; }

private:
    std::uint32_t seen_sessions_;
    bool in_session_ = false;
};
}  // namespace engine
