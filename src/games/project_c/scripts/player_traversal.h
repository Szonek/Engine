#pragma once
#include "traversal_math.h"

#include <engine.h>

#include <glm/glm.hpp>

#include <cstddef>

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
    // walking with the character controller (which also makes it fall); combat happens only here
    eGrounded,
    // attached to a wall, WASD moves on the wall
    eClimbing,
    // vault / mantle / climb down: follows a fixed path, input is ignored until it ends
    eScriptedMove
};

// The player's environment traversal: climbing walls, vaulting through windows, mantling onto roofs and climbing down
// roof edges (CLIMBING_AND_OCCLUSION.md). It probes the obstacles around the player, starts what the player walks into,
// moves the player while climbing or in a scripted move (setting its transform: the character controller isn't moved
// then) and plays the locomotion layer's clips for it, and shows the marker of the nearest traversal. It knows nothing
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
        // nothing keeps the player from starting a traversal (it isn't attacking, dodging or stunned)
        bool can_start = false;
    };
    // On the ground: shows the marker at the traversal ahead (or the nearest one) and starts the one the player walked
    // into. Returns the mode the player is in now: eGrounded when nothing started.
    MovementMode update_grounded(const GroundInput& input, float delta_seconds);
    // On a wall: `move_input` is the movement input in world space (as on the ground: toward the wall climbs up, away
    // climbs down, along it shimmies), `drop_pressed` lets go. Returns the mode the player is in now.
    MovementMode update_climbing(const glm::vec3& move_input, bool drop_pressed, float delta_seconds);
    // in a scripted move; returns the mode the player is in now
    MovementMode update_scripted_move(float delta_seconds);
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
        glm::vec3 marker{ 0.0f };           // where the indicator is shown
        glm::vec3 wall_normal{ 0.0f };
        glm::vec3 climb_position{ 0.0f };   // eClimb only
        TraversalMove move{};               // eVault and eClimbDown only
    };

    ProbeHit probe(const glm::vec3& from, const glm::vec3& to) const;
    TraversalPlan find_traversal(const glm::vec3& position, const glm::vec3& direction) const;
    TraversalPlan find_nearest_traversal(const glm::vec3& position) const;
    TraversalPlan plan_vault(const glm::vec3& position, const ProbeHit& low_hit) const;
    TraversalPlan plan_climb_down(const glm::vec3& position, const glm::vec3& direction) const;
    MovementMode execute_traversal(const TraversalPlan& plan);
    bool try_start_mantle(const glm::vec3& position);
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
    engine_game_object_t marker_ = ENGINE_INVALID_GAME_OBJECT_ID;
    TraversalPlan::Type marker_type_ = TraversalPlan::Type::eNone;
    float marker_time_ = 0.0f;
};
}  // namespace project_c
