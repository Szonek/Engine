#pragma once
#include "game_components.h"

#include <engine.h>

#include <glm/glm.hpp>

namespace engine
{
class IScene;
}

namespace project_c
{
// The player's shove (FALLS_AND_SHOVES.md): a kick on its own key with a strong push, which sends an enemy off a ledge
// to its death. It finds the enemy in front of the player (an overlap with the characters' collision layer) and pushes
// it at the kick's contact; the Player plays the kick (Unarmed_Melee_Attack_Kick, whose contact is an animation event),
// says when one may start and turns toward the target.
class PlayerShove
{
public:
    // the kick's clip, and when in it the foot lands (seconds)
    static constexpr const char* KICK_CLIP = "Unarmed_Melee_Attack_Kick";
    static constexpr float KICK_CONTACT_SECONDS = 0.3f;

    PlayerShove(engine::IScene* scene, engine_game_object_t player);

    // the nearest enemy (not dying, not being executed) within shove_range of `position` (flat) and within
    // shove_half_angle_degrees of `forward`; ENGINE_INVALID_GAME_OBJECT_ID for none
    engine_game_object_t find_target(const glm::vec3& position, const glm::vec3& forward, const PlayerTuning& tuning) const;

    // a kick starts on `target` (none: it pushes nothing, a miss)
    void begin(engine_game_object_t target) { target_ = target; }
    // the foot lands: the target, if it is still there, is pushed along `direction` (flat) and staggered; true when it
    // pushed one. Once per kick.
    bool contact(const glm::vec3& direction, const PlayerTuning& tuning);
    // the kick's target, until its contact
    engine_game_object_t target() const { return target_; }

private:
    engine::IScene* scene_;
    engine_game_object_t go_;
    engine_game_object_t target_ = ENGINE_INVALID_GAME_OBJECT_ID;
};
}  // namespace project_c
