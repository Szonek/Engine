#pragma once
#include <glm/glm.hpp>

#include <algorithm>
#include <optional>

// The rings showing the player's noise on the ground, like Mark of the Ninja's (ENEMY_AWARENESS.md): a faint one at the
// footstep radius pulsing with the steps while the player walks, nothing while it sneaks or stands, and a bright one
// for a loud noise. Free of the engine, so it is unit tested (engine_logic_tests); the Player draws them.
namespace project_c
{
class NoiseRings
{
public:
    // how long a ring stays on the ground
    static constexpr float FOOTSTEP_RING_SECONDS = 0.25f;
    static constexpr float LOUD_RING_SECONDS = 0.3f;

    struct Ring
    {
        glm::vec3 center{ 0.0f };
        float radius = 0.0f;
        float remaining_seconds = 0.0f;
    };

    // A frame of `delta_seconds` in which the player made `noise_radius` of noise at `position` (0: silent), loud or
    // footsteps. Footsteps pulse a ring every `footstep_interval_seconds` while `show_footsteps`; the first step after
    // a silence pulses at once.
    void update(float delta_seconds, float noise_radius, bool loud, const glm::vec3& position, bool show_footsteps, float footstep_interval_seconds)
    {
        age(footstep_, delta_seconds);
        age(loud_, delta_seconds);
        if (noise_radius <= 0.0f)
        {
            until_next_step_ = 0.0f;
            return;
        }
        if (loud)
        {
            loud_ = Ring{ position, noise_radius, LOUD_RING_SECONDS };
            return;
        }
        until_next_step_ -= delta_seconds;
        if (show_footsteps && until_next_step_ <= 0.0f)
        {
            footstep_ = Ring{ position, noise_radius, FOOTSTEP_RING_SECONDS };
            until_next_step_ = std::max(footstep_interval_seconds, 0.0f);
        }
    }

    const std::optional<Ring>& footstep_ring() const { return footstep_; }
    const std::optional<Ring>& loud_ring() const { return loud_; }

private:
    static void age(std::optional<Ring>& ring, float delta_seconds)
    {
        if (ring)
        {
            ring->remaining_seconds -= delta_seconds;
            if (ring->remaining_seconds <= 0.0f)
            {
                ring.reset();
            }
        }
    }

    std::optional<Ring> footstep_;
    std::optional<Ring> loud_;
    float until_next_step_ = 0.0f;
};
}  // namespace project_c
