#pragma once
#include "game_components.h"
#include "traversal_math.h"

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
// How the player moves, one mode at a time (like the movement modes of Unreal's Character Movement Component). Only
// Player::set_movement_mode changes it.
enum class MovementMode
{
    // walking with the character controller; combat happens only here
    eGrounded,
    // falling or leaping: moved by the character controller with a little air control, until it lands
    eAirborne,
    // attached to a wall, WASD moves on the wall
    eClimbing,
    // vault / mantle / climb down: follows a fixed path, input is ignored until it ends
    eScriptedMove
};

// The player's environment traversal: climbing walls, vaulting through windows, mantling onto roofs, climbing down or
// running off roof edges, leaping and falling (CLIMBING_AND_OCCLUSION.md). It probes the obstacles around the player,
// starts what the player walks into, moves the player while climbing or in a scripted move (setting its transform: the
// character controller isn't moved then) and in the air (with the character controller), plays the locomotion layer's
// clips for it, and shows the marker of the nearest traversal. It knows nothing
// about combat: the Player says when a traversal may start and applies the mode it returns (Player::set_movement_mode).
class PlayerTraversal
{
public:
    // `locomotion_layer`: the player's animation layer it plays its clips on
    PlayerTraversal(engine::IScene* scene, engine_game_object_t player, std::size_t locomotion_layer);
    // destroys the marker
    ~PlayerTraversal();
    PlayerTraversal(const PlayerTraversal&) = delete;
    PlayerTraversal& operator=(const PlayerTraversal&) = delete;

    // the marker showing the nearest traversal (a runtime object), when the player starts
    void create_marker();
    // probes ignore the weapon the player holds (ENGINE_INVALID_GAME_OBJECT_ID for none)
    void set_held_weapon(engine_game_object_t weapon) { held_weapon_ = weapon; }

    struct GroundInput
    {
        // the player's feet
        glm::vec3 position{ 0.0f };
        // where the player walks: flat and normalized, zero without movement input
        glm::vec3 move_direction{ 0.0f };
        // metres per second toward move_direction, actually moved in the last frame
        float approach_speed = 0.0f;
        // nothing keeps the player from starting a traversal (it isn't attacking, dodging or stunned)
        bool can_start = false;
        // a dodge was pressed which the player can do now: at an edge it is a leap
        bool leap_pressed = false;
    };
    // On the ground: shows the marker at the traversal ahead (or the nearest one) and starts the one the player walked
    // into, or leaps off the edge ahead. Returns the mode the player is in now: eGrounded when nothing started,
    // eAirborne for a leap (which takes the place of the dodge).
    MovementMode update_grounded(const GroundInput& input, const TraversalTuning& tuning, float delta_seconds);
    // On a wall: `move_input` is the movement input in world space (as on the ground: toward the wall climbs up, away
    // climbs down, along it shimmies), `drop_pressed` lets go. Returns the mode the player is in now.
    MovementMode update_climbing(const glm::vec3& move_input, bool drop_pressed, const TraversalTuning& tuning, float delta_seconds);
    // in a scripted move; returns the mode the player is in now
    MovementMode update_scripted_move(float delta_seconds);
    // The player walked off the ground (the character controller doesn't stand it on anything) with this horizontal
    // velocity; returns eAirborne.
    MovementMode start_falling(const glm::vec3& velocity);
    // In the air: steered by `move_direction` (flat, normalized, zero without input) with air control, a fraction of
    // the ground's `acceleration` toward `move_speed`. Returns eGrounded once it landed.
    MovementMode update_airborne(const glm::vec3& move_direction, float move_speed, float acceleration, const TraversalTuning& tuning,
        float delta_seconds);
    // the traversal ends from outside (the player died): hides the marker, the player falls
    void cancel();

private:
    struct TraversalMove
    {
        traversal_math::MovePath path;
        float duration = 0.0f;
        float elapsed = 0.0f;
        float yaw = 0.0f;
        bool climb_after = false;   // enter climbing when done (climb down from roof), otherwise back to the ground
        // the clip of the second segment, stretched over it (the run-up's jump over the edge); nullptr: the first clip
        // is stretched over the whole move
        const char* finish_animation = nullptr;
        bool finish_started = false;
    };

    struct ProbeHit
    {
        bool hit = false;           // anything (except the player and its weapon) was hit
        bool building = false;      // hit object is a BuildingPart
        bool climbable = false;     // hit object is a climbable BuildingPart
        engine_game_object_t go = ENGINE_INVALID_GAME_OBJECT_ID;
        glm::vec3 position{ 0.0f };
        glm::vec3 normal{ 0.0f };
    };

    // Traversal available in the movement direction. Shown by the marker while approaching, started automatically once
    // the player reaches the obstacle.
    struct TraversalPlan
    {
        enum class Type
        {
            eNone,
            eClimb,
            eVault,
            eClimbDown
        };
        Type type = Type::eNone;
        float distance = 0.0f;              // how far the player still has to go before the traversal starts
        float drop = 0.0f;                  // eClimbDown only: how far below the ground past the edge is (infinity: none)
        glm::vec3 marker{ 0.0f };           // where the indicator is shown
        glm::vec3 wall_normal{ 0.0f };
        glm::vec3 climb_position{ 0.0f };   // eClimb only
        TraversalMove move{};               // eVault and eClimbDown only, without its duration (tuned)
    };

    // an edge ahead: where the floor ends, and the drop behind it
    struct Edge
    {
        float distance = 0.0f;  // the first point ahead without floor
        float drop = 0.0f;      // how far below the player's feet the ground there is (infinity: none)
    };

    ProbeHit probe(const glm::vec3& from, const glm::vec3& to) const;
    std::optional<Edge> find_edge(const glm::vec3& position, const glm::vec3& direction) const;
    TraversalPlan find_traversal(const glm::vec3& position, const glm::vec3& direction) const;
    TraversalPlan find_nearest_traversal(const glm::vec3& position) const;
    TraversalPlan plan_vault(const glm::vec3& position, const ProbeHit& low_hit) const;
    TraversalPlan plan_climb_down(const glm::vec3& position, const glm::vec3& direction) const;
    MovementMode start_action(traversal_math::GroundAction action, const glm::vec3& position, const TraversalPlan& plan, const TraversalTuning& tuning);
    MovementMode start_leap(const glm::vec3& position, const glm::vec3& direction, float drop, const TraversalTuning& tuning);
    // The path from `start` (on the wall, facing `into_wall`) up the wall and onto the ledge behind its top, `split`
    // of the time going up: the top is looked for from `max_height` above `start` down to the knee, a building's flat
    // top with room to stand on it and nothing above the way up. nullopt without one (the mantle and the run-up).
    std::optional<traversal_math::MovePath> plan_ledge_move(const glm::vec3& start, const glm::vec3& into_wall, float max_height, float split) const;
    bool try_start_mantle(const glm::vec3& position, const TraversalTuning& tuning);
    // the move of a run-up from `position` at the wall of the climb plan: over its top, or up to the run-up height
    // and climbing; nullopt when the wall can't be run up (the player climbs it)
    std::optional<TraversalMove> plan_run_up(const glm::vec3& position, const TraversalPlan& plan, const TraversalTuning& tuning) const;
    MovementMode start_run_up(const TraversalMove& move, const glm::vec3& wall_normal);
    MovementMode start_climbing(const glm::vec3& wall_normal, const glm::vec3& position);
    MovementMode start_move(const TraversalMove& move, const char* animation_name);
    // back on the ground: no new traversal starts for `regrab_cooldown` seconds
    MovementMode end(float regrab_cooldown);
    void update_marker(const TraversalPlan& plan, float delta_seconds);
    void set_position_and_yaw(const glm::vec3& position, float yaw);

    engine::IScene* scene_;
    engine_game_object_t go_;
    std::size_t locomotion_layer_;
    engine_game_object_t held_weapon_ = ENGINE_INVALID_GAME_OBJECT_ID;

    TraversalMove move_{};
    glm::vec3 climb_wall_normal_{ 0.0f, 0.0f, 1.0f };
    float regrab_cooldown_ = 0.0f;
    // the obstacle the player reached and whether it was running then: decided when it reaches it, so speeding up
    // while pushing into it doesn't change what happens
    struct Approach
    {
        TraversalPlan::Type type = TraversalPlan::Type::eNone;
        bool running = false;
        // how long the input has pushed into it since
        float pushed_seconds = 0.0f;
    };
    std::optional<Approach> approach_;
    // in the air: the horizontal velocity, and the vertical one of the leap's launch (the character controller adds
    // the fall)
    glm::vec2 air_velocity_{ 0.0f };
    float launch_vertical_speed_ = 0.0f;
    engine_game_object_t marker_ = ENGINE_INVALID_GAME_OBJECT_ID;
    TraversalPlan::Type marker_type_ = TraversalPlan::Type::eNone;
    float marker_time_ = 0.0f;
};
}  // namespace project_c
