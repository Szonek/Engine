#pragma once
#include <algorithm>

// The player's momentum (FALLS_AND_SHOVES.md): a meter from 0 to 1 that killing from above or behind fills, which makes
// the player faster and the execution chain longer, and which drains while the player brawls on the ground. Runtime
// state, never saved, empty at the start of every level. Engine-free, unit tested.
namespace project_c
{
// its values (a data asset, assets/data/player/momentum.json)
struct MomentumTuning
{
    // what each fills the meter by (it stops at 1)
    float ground_execution_fill;
    float plunge_fill;
    // each execution after the first of a chain (in place of ground_execution_fill)
    float chain_link_fill;
    // an enemy the player kicked dies from its fall
    float fall_kill_fill;
    // at a full meter: move speed x (1 + speed_bonus), chain range x (1 + chain_range_bonus); linear in between
    float speed_bonus;
    float chain_range_bonus;
    // on the ground in combat (an alert enemy within combat_radius metres): drained per second; a hit taken drains
    // hit_drain at once
    float combat_drain_per_second;
    float hit_drain;
    float combat_radius;
};

enum class MomentumEvent
{
    eGroundExecution,
    ePlunge,
    eChainLink,
    eFallKill,
};

class PlayerMomentum
{
public:
    float value() const { return value_; }
    // it drained in the last update (the HUD shows it brighter)
    bool is_draining() const { return draining_; }

    void add(MomentumEvent event, const MomentumTuning& tuning)
    {
        float amount = 0.0f;
        switch (event)
        {
        case MomentumEvent::eGroundExecution: amount = tuning.ground_execution_fill; break;
        case MomentumEvent::ePlunge: amount = tuning.plunge_fill; break;
        case MomentumEvent::eChainLink: amount = tuning.chain_link_fill; break;
        case MomentumEvent::eFallKill: amount = tuning.fall_kill_fill; break;
        }
        value_ = std::clamp(value_ + amount, 0.0f, 1.0f);
    }

    // Each frame: on the ground in combat it drains; anywhere else (out of combat, on a wall, in the air, above the
    // fight on a roof) it holds.
    void update(float delta_seconds, bool on_ground_in_combat, const MomentumTuning& tuning)
    {
        draining_ = on_ground_in_combat && value_ > 0.0f;
        if (draining_)
        {
            value_ = std::max(0.0f, value_ - tuning.combat_drain_per_second * delta_seconds);
        }
    }

    // a hit taken
    void on_hit(const MomentumTuning& tuning) { value_ = std::max(0.0f, value_ - tuning.hit_drain); }

    float speed_multiplier(const MomentumTuning& tuning) const { return 1.0f + value_ * tuning.speed_bonus; }
    float chain_range_multiplier(const MomentumTuning& tuning) const { return 1.0f + value_ * tuning.chain_range_bonus; }

private:
    float value_ = 0.0f;
    bool draining_ = false;
};
}  // namespace project_c
