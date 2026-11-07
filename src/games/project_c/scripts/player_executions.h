#pragma once
#include "execution_math.h"
#include "game_components.h"
#include "player_traversal.h"

#include <engine.h>

#include <glm/glm.hpp>

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
// kills it silently, and the chain from one to the next. It finds the target (the nearest of the enemies around the
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
    // An attack press on the ground: executes the target find_ground_target gives (eExecuting), otherwise eGrounded
    // (a normal attack, which ends the chain).
    MovementMode try_ground(const Context& context, const ExecutionTuning& tuning);

    // what an execution's frame did
    struct Update
    {
        MovementMode mode = MovementMode::eExecuting;
        // the strike landed this frame (hit stop, camera shake, the kill noise)
        bool struck = false;
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
    // a move to the back of `target` taking `move_seconds`, then the strike
    MovementMode start(const Target& target, const Context& context, float move_seconds, const ExecutionTuning& tuning);
    void set_position_and_yaw(const glm::vec3& position, float yaw);

    engine::IScene* scene_;
    engine_game_object_t go_;
    std::size_t locomotion_layer_;

    // the execution in progress
    engine_game_object_t victim_ = ENGINE_INVALID_GAME_OBJECT_ID;
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
