#pragma once
#include "enemy_awareness.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

// The maths and rules of the player's executions (EXECUTIONS.md): who can be executed and from where, the aerial
// assassination's landing, plunge and knockdown, the chain window. Free of the engine, so it is unit tested
// (engine_logic_tests).
namespace project_c::execution_math
{
// Degrees between an enemy's facing and the direction from it to the player, flat: 0 in front, 180 right behind. 0
// when they stand at the same spot or the facing is flat zero.
inline float angle_from_facing(const glm::vec3& enemy_position, const glm::vec3& enemy_forward, const glm::vec3& player_position)
{
    const glm::vec2 to_player(player_position.x - enemy_position.x, player_position.z - enemy_position.z);
    const glm::vec2 facing(enemy_forward.x, enemy_forward.z);
    if (glm::length(to_player) <= 0.0f || glm::length(facing) <= 0.0f)
    {
        return 0.0f;
    }
    const float cosine = std::clamp(glm::dot(glm::normalize(to_player), glm::normalize(facing)), -1.0f, 1.0f);
    return glm::degrees(std::acos(cosine));
}

// what an execution check knows of one enemy
struct Candidate
{
    Awareness awareness = Awareness::eUnaware;
    // it sees the player this frame
    bool sees_player = false;
    // knocked down by a plunge: executable from any side, whatever it knows
    bool knocked_down = false;
    // dying, or already being executed
    bool unavailable = false;
    // flat metres to what the check measures from (the player, or the landing point of a plunge)
    float distance = 0.0f;
    // the player's feet above (positive) or below the enemy's
    float height_difference = 0.0f;
    // angle_from_facing of the player
    float angle_from_facing = 0.0f;
    // nothing of the level is between them (a ray cast)
    bool reachable = true;
};

// what an execution needs of its target
struct Rules
{
    float range = 0.0f;
    // the height difference allowed either way
    float max_height_difference = 0.0f;
    // the player must be at least this far round from the enemy's facing (0: anywhere); 180 minus the half angle of
    // the arc at its back
    float min_angle_from_facing = 0.0f;
    // any awareness, also seeing the player (the plunge)
    bool any_awareness = false;
    bool allow_knocked_down = true;
};

// The ground rule's "not noticed": unaware, or suspicious while it doesn't see the player.
inline bool unnoticed(const Candidate& candidate)
{
    return !candidate.sees_player
        && (candidate.awareness == Awareness::eUnaware || candidate.awareness == Awareness::eSuspicious);
}

inline bool executable(const Candidate& candidate, const Rules& rules)
{
    if (candidate.unavailable || !candidate.reachable || candidate.distance > rules.range
        || std::abs(candidate.height_difference) > rules.max_height_difference)
    {
        return false;
    }
    if (candidate.knocked_down)
    {
        return rules.allow_knocked_down;
    }
    if (rules.any_awareness)
    {
        return true;
    }
    return unnoticed(candidate) && candidate.angle_from_facing >= rules.min_angle_from_facing;
}

// The candidate to execute: the nearest executable one, ties broken by the one the player is most behind; nullopt
// when none is executable.
inline std::optional<std::size_t> pick_target(std::span<const Candidate> candidates, const Rules& rules)
{
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < candidates.size(); i++)
    {
        if (!executable(candidates[i], rules))
        {
            continue;
        }
        if (!best
            || candidates[i].distance < candidates[*best].distance
            || (candidates[i].distance == candidates[*best].distance && candidates[i].angle_from_facing > candidates[*best].angle_from_facing))
        {
            best = i;
        }
    }
    return best;
}

// where the player strikes from: `offset` behind the victim, on its back axis (flat), at the victim's height
inline glm::vec3 execution_spot(const glm::vec3& victim_position, const glm::vec3& victim_forward, float offset)
{
    glm::vec2 facing(victim_forward.x, victim_forward.z);
    facing = glm::length(facing) > 0.0f ? glm::normalize(facing) : glm::vec2(0.0f, 1.0f);
    return victim_position - glm::vec3(facing.x, 0.0f, facing.y) * offset;
}

// where and when a fall reaches a height
struct Landing
{
    glm::vec3 point{ 0.0f };
    float seconds = 0.0f;
};

// Where a body at `position` moving at `velocity` (metres per second) under `gravity` (positive, downward) comes down
// to `height`; nullopt when it never does (it is below it and not rising).
inline std::optional<Landing> predict_landing(const glm::vec3& position, const glm::vec3& velocity, float gravity, float height)
{
    const float g = std::max(gravity, 0.001f);
    // position.y + vy t - g t^2 / 2 = height, the later root (coming down)
    const float discriminant = velocity.y * velocity.y + 2.0f * g * (position.y - height);
    if (discriminant < 0.0f)
    {
        return std::nullopt;
    }
    const float seconds = (velocity.y + std::sqrt(discriminant)) / g;
    if (seconds < 0.0f)
    {
        return std::nullopt;
    }
    return Landing{ glm::vec3(position.x + velocity.x * seconds, height, position.z + velocity.z * seconds), seconds };
}

// seconds a straight move of `distance` takes at `speed`, never less than `min_seconds`
inline float move_seconds(float distance, float speed, float min_seconds)
{
    return std::max(speed > 0.0f ? distance / speed : 0.0f, min_seconds);
}

// how far around the plunge's target the other enemies are knocked down: `per_metre` of the drop, at most `max_radius`
inline float knockdown_radius(float drop, float per_metre, float max_radius)
{
    return std::clamp(drop * per_metre, 0.0f, max_radius);
}

// The execution chain's window: it opens when an execution or plunge ends; a press while it is open continues the chain.
class ChainWindow
{
public:
    void open(float seconds) { remaining_seconds_ = seconds; }
    void close() { remaining_seconds_ = 0.0f; }
    void update(float delta_seconds) { remaining_seconds_ = std::max(0.0f, remaining_seconds_ - delta_seconds); }
    bool is_open() const { return remaining_seconds_ > 0.0f; }

private:
    float remaining_seconds_ = 0.0f;
};
}  // namespace project_c::execution_math
