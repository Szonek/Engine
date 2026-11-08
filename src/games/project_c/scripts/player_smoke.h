#pragma once
#include "smoke.h"

#include <engine.h>

#include <glm/glm.hpp>

#include <optional>

namespace engine
{
class IScene;
}

namespace project_c
{
// The player's smoke bomb (SMOKE.md), owned by Player like PlayerShove: the cooldown, where a throw lands, and the cloud
// it spawns at the throw's release. The Player says when a throw may start (on the ground, not in another action) and
// plays the throw (Throw); the bomb leaves the hand throw_release_seconds into it.
class PlayerSmoke
{
public:
    static constexpr const char* THROW_CLIP = "Throw";

    PlayerSmoke(engine::IScene* scene, engine_game_object_t player);

    // each frame, wherever the player is
    void update(float delta_seconds);
    bool is_ready() const { return cooldown_remaining_seconds_ <= 0.0f; }
    float cooldown_remaining_seconds() const { return cooldown_remaining_seconds_; }

    // A throw starts from `player` at `cursor_point` (the point under the cursor): where it will land is decided now
    // (clamped to the throw range, on the ground found under it: under the cursor point itself when it is within the
    // range, so an overhang above it doesn't catch the bomb), and the cooldown starts.
    void begin(const glm::vec3& player, const glm::vec3& cursor_point, const SmokeTuning& tuning);
    // The bomb leaves the hand: its cloud pops where the throw lands (once per throw). False when no throw is pending.
    bool release(const SmokeTuning& tuning);
    // a throw cut short before its release: no cloud (the cooldown stays)
    void cancel() { landing_.reset(); }
    // where the pending throw lands
    const std::optional<glm::vec3>& landing() const { return landing_; }

private:
    engine::IScene* scene_;
    engine_game_object_t go_;
    float cooldown_remaining_seconds_ = 0.0f;
    std::optional<glm::vec3> landing_;
};
}  // namespace project_c
