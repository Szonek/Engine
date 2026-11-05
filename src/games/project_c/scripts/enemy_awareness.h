#pragma once
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

// How an enemy notices the player (ENEMY_AWARENESS.md): a detection meter filled by what it sees and hears, and the
// awareness state it leads to. Free of the engine, so it is unit tested (engine_logic_tests); the Enemy script gathers
// the stimuli each frame and acts by the state.
namespace project_c
{
// How an enemy perceives the player: part of its enemy_type data asset (EnemyType::perception). Metres, seconds,
// degrees.
struct EnemyPerception
{
    // sight: a cone in front of the eyes, and all around within close_range
    float sight_range;
    float sight_half_angle_degrees;
    float close_range;
    float eye_height;
    // seconds the meter takes to fill while the player is seen at distance 0 / at sight_range (linear in between)
    float detect_seconds_near;
    float detect_seconds_far;
    // a player above the enemy's reach (on a roof) fills the meter this many times as fast (below 1: slower)
    float above_sight_multiplier;
    // hearing: seconds heard footsteps take to fill the meter, and what a loud noise adds at once
    float hearing_fill_seconds;
    float loud_noise_fill;
    // a full meter empties in this long when nothing is perceived
    float meter_decay_seconds;
    // seconds of searching before giving up; a pack calms down when no member perceived the player this long
    float lose_seconds;
    // metres per second while suspicious (walking to what it noticed) and searching
    float suspicious_move_speed;
    // seconds an alert enemy keeps hunting a player it stopped perceiving before it searches (a frame without line of
    // sight doesn't stop a chase)
    float lose_sight_grace_seconds;
};

enum class Awareness
{
    // idle, nothing noticed
    eUnaware,
    // the meter is filling: turns to the stimulus and walks to it ("?")
    eSuspicious,
    // the meter is full: chases and attacks ("!")
    eAlert,
    // lost the player: walks to the last known position and looks around ("?")
    eSearching
};

// what an enemy perceived in one frame
struct EnemyStimuli
{
    bool seen = false;
    float seen_distance = 0.0f;
    // the player is above the enemy's reach (on a roof)
    bool seen_above = false;
    glm::vec3 seen_position{ 0.0f };
    bool heard_footsteps = false;
    glm::vec3 footsteps_position{ 0.0f };
    bool heard_loud_noise = false;
    glm::vec3 loud_noise_position{ 0.0f };
    // hit by the player
    bool damaged = false;
    glm::vec3 damage_position{ 0.0f };
    // another member of its pack is alert (and perceived the player recently)
    bool pack_alert = false;
    glm::vec3 pack_position{ 0.0f };
};

// The player at `target` is in view of an enemy standing at `position` facing `forward`: within sight_range and the
// sight half angle of the facing, or within close_range in any direction (flat, in XZ). It still has to be visible
// (a ray cast, the Enemy's job).
inline bool in_sight_cone(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& target, const EnemyPerception& perception)
{
    const glm::vec2 delta(target.x - position.x, target.z - position.z);
    const float distance = glm::length(delta);
    if (distance <= perception.close_range)
    {
        return true;
    }
    const glm::vec2 facing(forward.x, forward.z);
    if (distance > perception.sight_range || glm::length(facing) <= 0.0f)
    {
        return false;
    }
    return glm::dot(delta / distance, glm::normalize(facing)) >= std::cos(glm::radians(perception.sight_half_angle_degrees));
}

// Seconds the meter takes to fill while the player is seen `distance` away: detect_seconds_near at 0 to
// detect_seconds_far at sight_range, divided by above_sight_multiplier for a player above the enemy's reach (infinite
// when it is 0: never seen up there).
inline float sight_fill_seconds(float distance, const EnemyPerception& perception, bool above)
{
    const float t = perception.sight_range > 0.0f ? std::clamp(distance / perception.sight_range, 0.0f, 1.0f) : 0.0f;
    const float seconds = perception.detect_seconds_near + (perception.detect_seconds_far - perception.detect_seconds_near) * t;
    if (!above)
    {
        return seconds;
    }
    return perception.above_sight_multiplier > 0.0f ? seconds / perception.above_sight_multiplier : std::numeric_limits<float>::infinity();
}

// One enemy's awareness of the player: its detection meter (0 to 1), its state and where it last noticed the player.
// The transitions are all in update.
class EnemyAwareness
{
public:
    void update(float delta_seconds, const EnemyStimuli& stimuli, const EnemyPerception& perception)
    {
        perceived_ = stimuli.seen || stimuli.heard_footsteps || stimuli.heard_loud_noise || stimuli.damaged;
        seconds_since_perceived_ = perceived_ ? 0.0f : seconds_since_perceived_ + delta_seconds;

        // the pack's position first: what the enemy perceives itself this frame is newer
        if (stimuli.pack_alert)
        {
            last_known_position_ = stimuli.pack_position;
        }
        if (stimuli.heard_footsteps)
        {
            meter_ += fill(perception.hearing_fill_seconds, delta_seconds);
            last_known_position_ = stimuli.footsteps_position;
        }
        if (stimuli.heard_loud_noise)
        {
            meter_ += perception.loud_noise_fill;
            last_known_position_ = stimuli.loud_noise_position;
        }
        if (stimuli.seen)
        {
            meter_ += fill(sight_fill_seconds(stimuli.seen_distance, perception, stimuli.seen_above), delta_seconds);
            last_known_position_ = stimuli.seen_position;
        }
        if (stimuli.damaged)
        {
            last_known_position_ = stimuli.damage_position;
        }
        if (stimuli.damaged || stimuli.pack_alert)
        {
            meter_ = 1.0f;
        }

        switch (state_)
        {
        case Awareness::eUnaware:
        case Awareness::eSuspicious:
            if (!perceived_ && !stimuli.pack_alert)
            {
                meter_ -= fill(perception.meter_decay_seconds, delta_seconds);
            }
            meter_ = std::clamp(meter_, 0.0f, 1.0f);
            if (meter_ >= 1.0f)
            {
                set_state(Awareness::eAlert);
            }
            else
            {
                set_state(meter_ > 0.0f ? Awareness::eSuspicious : Awareness::eUnaware);
            }
            break;
        case Awareness::eAlert:
            // the meter stays full while it hunts; it searches once it hasn't perceived the player for the grace time
            meter_ = 1.0f;
            if (!perceived_ && !stimuli.pack_alert && seconds_since_perceived_ >= perception.lose_sight_grace_seconds)
            {
                set_state(Awareness::eSearching);
            }
            break;
        case Awareness::eSearching:
            meter_ = 1.0f;
            if (perceived_ || stimuli.pack_alert)
            {
                set_state(Awareness::eAlert);
            }
            else
            {
                searching_seconds_ += delta_seconds;
                if (searching_seconds_ >= perception.lose_seconds)
                {
                    // gives up: the meter decays from full again
                    set_state(Awareness::eSuspicious);
                }
            }
            break;
        }
    }

    Awareness state() const { return state_; }
    float meter() const { return meter_; }
    // where it last noticed the player (or was told to by its pack)
    const glm::vec3& last_known_position() const { return last_known_position_; }
    // it saw, heard or was hit by the player in the last update (not counting its pack)
    bool perceived_this_frame() const { return perceived_; }
    // since it last perceived the player itself (infinite when it never did)
    float seconds_since_perceived() const { return seconds_since_perceived_; }
    // alert and it perceived the player itself within the grace time: it chases and attacks the player where it is
    // (otherwise it goes to the last known position)
    bool hunting(const EnemyPerception& perception) const
    {
        return state_ == Awareness::eAlert && seconds_since_perceived_ <= perception.lose_sight_grace_seconds;
    }

private:
    // what a meter filling (or emptying) in `seconds` changes by in `delta_seconds`: all of it at once for 0 seconds
    static float fill(float seconds, float delta_seconds)
    {
        return seconds > 0.0f ? delta_seconds / seconds : 1.0f;
    }

    void set_state(Awareness state)
    {
        if (state != state_)
        {
            state_ = state;
            searching_seconds_ = 0.0f;
        }
    }

    Awareness state_ = Awareness::eUnaware;
    float meter_ = 0.0f;
    glm::vec3 last_known_position_{ 0.0f };
    bool perceived_ = false;
    float seconds_since_perceived_ = std::numeric_limits<float>::infinity();
    float searching_seconds_ = 0.0f;
};
}  // namespace project_c
