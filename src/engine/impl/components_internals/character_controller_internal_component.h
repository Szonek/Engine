#pragma once

namespace engine
{
// Runtime state of a character controller: added and removed with its component, never saved.
struct character_controller_internal_component_t
{
    // vertical speed from gravity (and upward moves), in m/s; reset when the character lands or hits a ceiling
    float vertical_speed = 0.0f;
};
}  // namespace engine
