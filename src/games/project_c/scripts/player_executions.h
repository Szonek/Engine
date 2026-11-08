#pragma once
#include "execution_math.h"
#include "game_components.h"
#include "player_traversal.h"

#include <engine.h>

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace engine
{
class IScene;
}

namespace project_c
{
class Enemy;

// The player's executions (EXECUTIONS.md): a lunge to the back of an enemy which hasn't noticed it and a strike which
// kills it silently, the plunge from the air onto an enemy below (the aerial assassination), and the chain from one to
// the next. It finds the target (the nearest of the enemies around the
// player which may be executed), runs the execution in MovementMode::eExecuting (setting the player's transform: the character
// controller isn't moved then) and plays the locomotion layer's clip for it. The Player says when one may start and
// applies what it returns (the mode, the strike's hit stop and noise).
class PlayerExecutions
{
public:
    // `locomotion_layer`: the player's animation layer it plays its clips on
    PlayerExecutions(engine::IScene* scene, engine_game_object_t player, std::size_t locomotion_layer);

    // what the player is now
    struct Context
    {
        // the feet
        glm::vec3 position{ 0.0f };
        // it holds a weapon (the daggers' clip; unarmed it kicks)
        bool armed = false;
    };

    // The target an attack press on the ground would execute now (the next one of the chain while its window is
    // open), or ENGINE_INVALID_GAME_OBJECT_ID; what the HUD marks. Call each grounded frame the player can act.
    engine_game_object_t find_ground_target(const Context& context, const ExecutionTuning& tuning) const;
    // An attack press on the ground: executes `target`, what find_ground_target gave this frame (eExecuting); with none,
    // eGrounded (a normal attack, which ends the chain).
    MovementMode try_ground(const Context& context, const ExecutionTuning& tuning, engine_game_object_t target);

    // In the air, every frame: where the player is, so the plunge knows where it would land.
    void observe_air(const glm::vec3& position, float delta_seconds);
    // not in the air (any more): the next fall starts measuring again
    void leave_air() { air_position_.reset(); }
    // The enemy an attack press in the air would plunge onto now: the nearest to where the player would land, in any
    // awareness state, while the player is at least aerial_min_drop above the ground; ENGINE_INVALID_GAME_OBJECT_ID
    // for none. `knockdown_radius` is the radius the plunge would knock the others down in.
    engine_game_object_t find_plunge_target(const glm::vec3& position, const ExecutionTuning& tuning, float* knockdown_radius = nullptr) const;
    // An attack press in the air: plunges onto find_plunge_target's enemy (eExecuting), otherwise eAirborne (the
    // press waits for the landing).
    MovementMode try_plunge(const glm::vec3& position, const ExecutionTuning& tuning);
    // the landing after a plunge recovered: the chain window opens
    void open_chain_window(const ExecutionTuning& tuning) { chain_window_.open(tuning.chain_window_seconds); }

    // what an execution's frame did
    struct Update
    {
        MovementMode mode = MovementMode::eExecuting;
        // the strike landed this frame (hit stop, camera shake, the kill noise)
        bool struck = false;
        // the plunge hit its target: the player lands (a hard landing)
        bool plunged = false;
    };
    // in eExecuting
    Update update(const ExecutionTuning& tuning, float delta_seconds);

    // on the ground: the chain window closes after its time
    void update_chain_window(float delta_seconds) { chain_window_.update(delta_seconds); }
    bool is_chain_window_open() const { return chain_window_.is_open(); }
    // the victim of the execution in progress (ENGINE_INVALID_GAME_OBJECT_ID for none)
    engine_game_object_t victim() const { return victim_; }

private:
    struct Target
    {
        engine_game_object_t go = ENGINE_INVALID_GAME_OBJECT_ID;
        Enemy* enemy = nullptr;
    };
    // of the enemies within `range` of `center` (flat), the nearest executable by `rules` (measured from `from`)
    Target pick(const glm::vec3& center, float range, const execution_math::Rules& rules, const glm::vec3& from) const;
    // where the player strikes `target` from: behind it, or (when a wall is there) on the side the player comes from
    glm::vec3 strike_spot(engine_game_object_t target, const glm::vec3& from) const;
    // a move to `spot` taking `move_seconds`, then the strike
    MovementMode start(const Target& target, const Context& context, const glm::vec3& spot, float move_seconds, const ExecutionTuning& tuning);
    void set_position_and_yaw(const glm::vec3& position, float yaw);

    engine::IScene* scene_;
    engine_game_object_t go_;
    std::size_t locomotion_layer_;

    // the execution in progress
    engine_game_object_t victim_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // a plunge (onto the victim, then a landing) rather than a strike from behind
    bool plunge_ = false;
    float knockdown_radius_ = 0.0f;
    // in the air: the last position and the velocity it gives
    std::optional<glm::vec3> air_position_;
    glm::vec3 air_velocity_{ 0.0f };
    glm::vec3 move_from_{ 0.0f };
    glm::vec3 move_to_{ 0.0f };
    float yaw_ = 0.0f;
    float elapsed_ = 0.0f;
    float move_seconds_ = 0.0f;
    // when in it the clip starts (after a long dash), the strike lands and it ends
    float clip_start_ = 0.0f;
    float strike_at_ = 0.0f;
    float end_at_ = 0.0f;
    const char* clip_ = nullptr;
    bool clip_started_ = false;
    bool struck_ = false;
    execution_math::ChainWindow chain_window_;
};
}  // namespace project_c
