#pragma once
#include "engine.h"

#include <cstdint>

namespace engine
{
// Follows the engine's play sessions from one update to the next, for the application and each scene: a Stop moved
// enginePlayModeGetSessionCount (the session ended), and a session begins when the game is out of Edit mode (Play, or
// the first update of an application which starts playing). A Stop and a Play between two updates end the old session
// and begin the new one in the same update.
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
        if (const auto sessions = enginePlayModeGetSessionCount(); sessions != seen_sessions_)
        {
            seen_sessions_ = sessions;
            changes.ended = in_session_;
            in_session_ = false;
        }
        if (!in_session_ && enginePlayModeGetState() != ENGINE_PLAY_MODE_EDIT)
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
