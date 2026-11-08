#pragma once
#include <algorithm>

// Fall damage (FALLS_AND_SHOVES.md), the same rule for the player and the enemies: no damage up to a safe height, then
// a share of the max HP rising linearly to all of it at a lethal height, like Assassin's Creed. Engine-free, unit
// tested.
namespace project_c
{
// the curve of a kind of character (a game struct type: the enemy type's and the traversal tuning's `fall`)
struct FallDamage
{
    // a drop up to this height (metres) does no damage
    float min_height;
    // a drop from this height or higher kills
    float lethal_height;
};

// The share of the max HP (0..1) a fall of `drop` metres takes: 0 up to min_height, rising linearly to 1 at
// lethal_height. A roll (`rolled`) halves it. A curve whose lethal height isn't above its min height kills from the min
// height on.
inline float fall_damage_fraction(const FallDamage& curve, float drop, bool rolled)
{
    if (!(drop > curve.min_height))
    {
        return 0.0f;
    }
    const float range = curve.lethal_height - curve.min_height;
    const float fraction = range > 0.0f ? std::min(1.0f, (drop - curve.min_height) / range) : 1.0f;
    return rolled ? fraction * 0.5f : fraction;
}

// The drop of a fall, measured from its highest point: a fall which starts going up (a leap, a knockback off a ledge)
// counts from its top.
class FallMeasure
{
public:
    // the fall starts at `height`
    void start(float height)
    {
        highest_ = height;
        falling_ = true;
    }
    // where the character is now, each frame of the fall
    void update(float height)
    {
        highest_ = std::max(highest_, height);
    }
    bool is_falling() const { return falling_; }
    // the fall ends at `height`: how far it dropped (0 when it ended higher than it started)
    float land(float height)
    {
        falling_ = false;
        return std::max(0.0f, highest_ - height);
    }

private:
    float highest_ = 0.0f;
    bool falling_ = false;
};
}  // namespace project_c
