#pragma once
#include <glm/glm.hpp>

#include <algorithm>

// The smoke bomb (SMOKE.md): a cloud which hides the player from the enemies' sight and blinds the ones it pops on.
// Its rules, engine-free and unit tested (smoke_tests.cpp in engine_logic_tests).
namespace project_c
{
// its values (a data asset, assets/data/player/smoke_bomb.json): seconds and metres
struct SmokeTuning
{
    // seconds from a throw until the next one
    float cooldown_seconds;
    // the farthest the bomb lands from the player (flat)
    float throw_range;
    // the cloud: a sphere this big, its centre this high above where the bomb landed
    float radius;
    float centre_height;
    // it grows to its radius in grow_seconds, lasts duration_seconds in all, shrinking to nothing over the last
    // fade_seconds
    float grow_seconds;
    float duration_seconds;
    float fade_seconds;
    // the enemies within the radius when it pops can't see or act for this long
    float blind_seconds;
    // the pop is a loud noise this far
    float noise_radius;
    // the throw: the bomb leaves the hand this long after it starts, and the player acts again this long after that
    // (the Throw clip's long follow-through is cut short)
    float throw_release_seconds;
    float throw_recovery_seconds;
};

// the cloud's radius `age` seconds after it popped: growing, full, then fading; 0 before and after its life
inline float cloud_radius(float age, const SmokeTuning& tuning)
{
    if (age < 0.0f || age >= tuning.duration_seconds || tuning.radius <= 0.0f)
    {
        return 0.0f;
    }
    float scale = 1.0f;
    if (tuning.grow_seconds > 0.0f && age < tuning.grow_seconds)
    {
        scale = age / tuning.grow_seconds;
    }
    const float left = tuning.duration_seconds - age;
    if (tuning.fade_seconds > 0.0f && left < tuning.fade_seconds)
    {
        scale = std::min(scale, left / tuning.fade_seconds);
    }
    return tuning.radius * std::clamp(scale, 0.0f, 1.0f);
}

// whether the segment from `a` to `b` touches (or is inside) the sphere: what a cloud does to a line of sight
inline bool segment_touches_sphere(const glm::vec3& a, const glm::vec3& b, const glm::vec3& centre, float radius)
{
    if (!(radius > 0.0f))
    {
        return false;
    }
    const glm::vec3 segment = b - a;
    const float length_squared = glm::dot(segment, segment);
    // the point of the segment nearest the centre
    const float t = length_squared > 0.0f ? std::clamp(glm::dot(centre - a, segment) / length_squared, 0.0f, 1.0f) : 0.0f;
    const glm::vec3 nearest = a + segment * t;
    const glm::vec3 offset = centre - nearest;
    return glm::dot(offset, offset) <= radius * radius;
}

// where a bomb thrown from `player` at the point under the cursor lands: there, or at throw_range along the same
// direction (flat) when farther; the height is the cursor point's (the ground is found under it afterwards)
inline glm::vec3 smoke_landing(const glm::vec3& player, const glm::vec3& cursor_point, float throw_range)
{
    const glm::vec2 offset(cursor_point.x - player.x, cursor_point.z - player.z);
    const float distance = glm::length(offset);
    if (distance <= throw_range || distance <= 0.0f)
    {
        return cursor_point;
    }
    const glm::vec2 clamped = offset * (throw_range / distance);
    return glm::vec3(player.x + clamped.x, cursor_point.y, player.z + clamped.y);
}
}  // namespace project_c
