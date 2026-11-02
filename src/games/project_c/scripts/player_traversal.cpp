#include "player_traversal.h"
#include "building_script.h"
#include "scripts_utils.h"

#include "../app.h"
#include "../prefab_types.h"
#include "iscene.h"
#include <utils.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
// Heights are measured from the player's feet (transform origin), player is ~0.85 tall.
constexpr float PLAYER_HEIGHT = 0.85f;
constexpr float PLAYER_HALF_WIDTH = 0.18f;
constexpr float PROBE_KNEE_HEIGHT = 0.3f;
constexpr float PROBE_CHEST_HEIGHT = 0.7f;
constexpr float PROBE_HEAD_HEIGHT = 0.95f;
constexpr float TRAVERSAL_DETECT_RANGE = 1.3f;          // obstacles this close in the movement direction show the marker
constexpr float TRAVERSAL_TRIGGER_DISTANCE = 0.35f;     // traversal starts automatically this close to the obstacle
constexpr float TRAVERSAL_MIN_APPROACH_DOT = 0.6f;      // movement has to point into the wall (not just walk along it)
constexpr float TRAVERSAL_REGRAB_COOLDOWN = 0.3f;
constexpr float TRAVERSAL_DROP_REGRAB_COOLDOWN = 0.8f;  // after letting go with SPACE
constexpr float CLIMB_INPUT_DEADZONE = 0.3f;
constexpr float CLIMB_WALL_OFFSET = 0.2f;               // distance between wall surface and the player while climbing
constexpr float CLIMB_PROBE_DEPTH = 0.25f;              // how far behind the expected wall surface probes still count as a hit
constexpr float CLIMB_DROP_PUSH = 0.15f;
constexpr float MANTLE_LEDGE_DEPTH = 0.25f;             // how far behind the wall surface ledge is looked for
constexpr float MANTLE_MAX_LEDGE_HEIGHT = 1.4f;
constexpr float VAULT_MAX_HEIGHT = 0.7f;
constexpr float VAULT_MAX_DEPTH = 1.0f;
constexpr float VAULT_CLEARANCE = 0.12f;
constexpr float VAULT_LANDING_DISTANCE = 0.35f;
constexpr float VAULT_MAX_ALIGNMENT = 0.6f;
constexpr float CLIMB_DOWN_EDGE_DISTANCE = 0.45f;
constexpr float CLIMB_DOWN_HANG_DEPTH = 0.95f;
// where edges are looked for ahead of the player: the first point without floor is the edge; the furthest is the reach
// of a leap
constexpr std::array<float, 3> EDGE_SAMPLE_DISTANCES = { CLIMB_DOWN_EDGE_DISTANCE, CLIMB_DOWN_EDGE_DISTANCE + 0.4f, CLIMB_DOWN_EDGE_DISTANCE + 0.8f };
// how deep the ground below an edge is looked for
constexpr float EDGE_DROP_PROBE_DEPTH = 20.0f;
// No dedicated climbing clips in the KayKit pack, closest matches are used.
constexpr const char* CLIMB_ANIMATION = "Jump_Idle";
constexpr const char* VAULT_ANIMATION = "Jump_Full_Short";
constexpr const char* MANTLE_ANIMATION = "Jump_Full_Short";
constexpr const char* LEAP_ANIMATION = "Jump_Start";
// the run-up: running up the wall (sped up), then the jump over its edge
constexpr const char* RUN_UP_ANIMATION = "Running_A";
constexpr const char* RUN_UP_FINISH_ANIMATION = "Jump_Full_Short";
constexpr float RUN_UP_ANIMATION_RATE = 1.6f;
// the part of the run-up spent on the wall, the rest goes over the edge
constexpr float RUN_UP_WALL_FRACTION = 0.7f;
constexpr const char* AIRBORNE_ANIMATION = "Jump_Idle";
constexpr float CLIMB_ANIMATION_RATE_MOVING = 1.6f;
constexpr float CLIMB_ANIMATION_RATE_IDLE = 0.2f;
constexpr float TRAVERSAL_MARKER_SIZE = 0.05f;           // Primitive_Cube_Small is 2 units big, pivot at the bottom

const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);

glm::vec3 flatten(const glm::vec3& v)
{
    const glm::vec3 flat(v.x, 0.0f, v.z);
    const float length = glm::length(flat);
    return length > 0.0001f ? flat / length : glm::vec3(0.0f);
}

float yaw_from_direction(const glm::vec3& direction)
{
    return std::atan2(direction.x, direction.z);
}
}  // namespace

project_c::PlayerTraversal::PlayerTraversal(engine::IScene* scene, engine_game_object_t player, std::size_t locomotion_layer)
    : scene_(scene)
    , go_(player)
    , locomotion_layer_(locomotion_layer)
{
}

project_c::PlayerTraversal::~PlayerTraversal()
{
    if (engineGameObjectIsValid(marker_))
    {
        engineGameObjectDestroy(marker_);
    }
}

project_c::MovementMode project_c::PlayerTraversal::update_grounded(const GroundInput& input, const TraversalTuning& tuning, float delta_seconds)
{
    regrab_cooldown_ = std::max(0.0f, regrab_cooldown_ - delta_seconds);
    const bool moving = glm::length(input.move_direction) > 0.0f;

    // a dodge toward an edge leaps off it
    if (input.leap_pressed && moving)
    {
        if (const auto edge = find_edge(input.position, input.move_direction))
        {
            approach_.reset();
            update_marker({}, delta_seconds);
            return start_leap(input.position, input.move_direction, edge->drop, tuning);
        }
    }

    // walking into a wall / window / roof edge: show marker while approaching, start traversal on contact
    TraversalPlan plan{};
    if (input.can_start && moving)
    {
        plan = find_traversal(input.position, input.move_direction);
    }
    // marker stays visible whenever player is close to something traversable, even when standing still or walking along it
    update_marker(plan.type != TraversalPlan::Type::eNone ? plan : find_nearest_traversal(input.position), delta_seconds);
    if (plan.type == TraversalPlan::Type::eNone || plan.distance > TRAVERSAL_TRIGGER_DISTANCE)
    {
        approach_.reset();
        return MovementMode::eGrounded;
    }
    if (!approach_ || approach_->type != plan.type)
    {
        approach_ = Approach{ plan.type, input.approach_speed >= tuning.run_min_speed, 0.0f };
    }
    else
    {
        approach_->pushed_seconds += delta_seconds;
    }
    if (regrab_cooldown_ > 0.0f)
    {
        return MovementMode::eGrounded;
    }

    traversal_math::GroundSituation situation{};
    switch (plan.type)
    {
    case TraversalPlan::Type::eClimb: situation.obstacle = traversal_math::Obstacle::eWall; break;
    case TraversalPlan::Type::eVault: situation.obstacle = traversal_math::Obstacle::eWindow; break;
    case TraversalPlan::Type::eClimbDown: situation.obstacle = traversal_math::Obstacle::eEdge; break;
    default: break;
    }
    situation.running = approach_->running;
    situation.drop = plan.drop;
    situation.pushed_seconds = approach_->pushed_seconds;
    const auto action = traversal_math::choose_ground_action(situation, tuning.max_safe_drop, tuning.climb_intent_seconds);
    return start_action(action, input.position, plan, tuning);
}

void project_c::PlayerTraversal::cancel()
{
    update_marker({}, 0.0f);
}

project_c::PlayerTraversal::ProbeHit project_c::PlayerTraversal::probe(const glm::vec3& from, const glm::vec3& to) const
{
    // triggers (the attack trigger) are skipped anyway
    std::array<engine_game_object_t, 2> ignore_list = { go_, ENGINE_INVALID_GAME_OBJECT_ID };
    if (engineGameObjectIsValid(held_weapon_))
    {
        ignore_list[1] = held_weapon_;
    }
    engine_ray_cast_filter_t filter{};
    filter.ignore = ignore_list.data();
    filter.ignore_count = ignore_list.size();

    const glm::vec3 direction = to - from;
    engine_ray_hit_t hit_info{};
    ProbeHit ret{};
    if (!enginePhysicsRayCast(glm::value_ptr(from), glm::value_ptr(direction), glm::length(direction), &filter, &hit_info))
    {
        return ret;
    }
    ret.hit = true;
    ret.go = hit_info.go;
    ret.position = glm::make_vec3(hit_info.position);
    ret.normal = glm::make_vec3(hit_info.normal);
    if (const auto* part = scene_->get_script<BuildingPart>(hit_info.go))
    {
        ret.building = true;
        ret.climbable = part->is_climbable(ret.normal);
    }
    return ret;
}

project_c::PlayerTraversal::TraversalPlan project_c::PlayerTraversal::find_traversal(const glm::vec3& position, const glm::vec3& direction) const
{
    const glm::vec3 knee_from = position + WORLD_UP * PROBE_KNEE_HEIGHT;
    const glm::vec3 chest_from = position + WORLD_UP * PROBE_CHEST_HEIGHT;
    const auto knee_hit = probe(knee_from, knee_from + direction * TRAVERSAL_DETECT_RANGE);
    const auto chest_hit = probe(chest_from, chest_from + direction * TRAVERSAL_DETECT_RANGE);
    const auto distance_to = [&](const ProbeHit& h) { return glm::dot(h.position - position, direction); };
    // vertical face of a building, approached head-on (walking along a wall does not trigger anything)
    const auto is_wall_ahead = [&](const ProbeHit& h)
        {
            return h.building
                && std::abs(h.normal.y) < 0.3f
                && glm::dot(direction, -flatten(h.normal)) >= TRAVERSAL_MIN_APPROACH_DOT;
        };

    // something low in front, free space above -> vault over (window sill, low wall)
    if (is_wall_ahead(knee_hit) && (!chest_hit.hit || distance_to(chest_hit) > distance_to(knee_hit) + 0.1f))
    {
        auto plan = plan_vault(position, knee_hit);
        plan.distance = distance_to(knee_hit);
        return plan;
    }

    // climbable wall in front
    if (is_wall_ahead(chest_hit) && chest_hit.climbable)
    {
        TraversalPlan plan{};
        plan.type = TraversalPlan::Type::eClimb;
        plan.distance = distance_to(chest_hit);
        plan.wall_normal = flatten(chest_hit.normal);
        plan.climb_position = chest_hit.position + plan.wall_normal * CLIMB_WALL_OFFSET;
        plan.climb_position.y = position.y + 0.05f;
        // where the hands grab the wall
        plan.marker = chest_hit.position + plan.wall_normal * TRAVERSAL_MARKER_SIZE + WORLD_UP * (PROBE_HEAD_HEIGHT - PROBE_CHEST_HEIGHT);
        return plan;
    }

    // walking toward the edge of a roof
    return plan_climb_down(position, direction);
}

project_c::PlayerTraversal::TraversalPlan project_c::PlayerTraversal::find_nearest_traversal(const glm::vec3& position) const
{
    // 8 directions, 45 degrees apart, so every wall is approached within TRAVERSAL_MIN_APPROACH_DOT by one of them
    constexpr int directions_count = 8;
    TraversalPlan nearest{};
    for (int i = 0; i < directions_count; i++)
    {
        const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(directions_count);
        const auto plan = find_traversal(position, glm::vec3(std::sin(angle), 0.0f, std::cos(angle)));
        if (plan.type != TraversalPlan::Type::eNone && (nearest.type == TraversalPlan::Type::eNone || plan.distance < nearest.distance))
        {
            nearest = plan;
        }
    }
    return nearest;
}

project_c::PlayerTraversal::TraversalPlan project_c::PlayerTraversal::plan_vault(const glm::vec3& position, const ProbeHit& low_hit) const
{
    const glm::vec3 wall_normal = flatten(low_hit.normal);
    const glm::vec3 forward = -wall_normal;
    const glm::vec3 tangent = glm::normalize(glm::cross(WORLD_UP, wall_normal));

    // obstacle depth: cast back toward the player from the far side
    const auto far_hit = probe(low_hit.position + forward * VAULT_MAX_DEPTH, low_hit.position);
    if (!far_hit.building)
    {
        return {};
    }
    const float depth = glm::dot(far_hit.position - low_hit.position, forward);

    // top of the obstacle
    const glm::vec3 middle = low_hit.position + forward * (depth * 0.5f);
    const auto top_hit = probe(glm::vec3(middle.x, position.y + VAULT_MAX_HEIGHT, middle.z), glm::vec3(middle.x, position.y, middle.z));
    if (!top_hit.hit || top_hit.normal.y < 0.7f)
    {
        return {};
    }
    const float top_y = top_hit.position.y;

    // body has to fit above the obstacle (i.e. window opening height)
    if (probe(top_hit.position + WORLD_UP * 0.02f, top_hit.position + WORLD_UP * PLAYER_HEIGHT).hit)
    {
        return {};
    }

    // align with the window opening
    float lateral_offset = 0.0f;
    if (const auto* part = scene_->get_script<BuildingPart>(low_hit.go))
    {
        if (const auto alignment_point = part->get_alignment_point())
        {
            const float offset = glm::dot(alignment_point.value() - position, tangent);
            if (std::abs(offset) <= VAULT_MAX_ALIGNMENT)
            {
                lateral_offset = offset;
            }
        }
    }

    // landing on the other side
    const glm::vec3 landing = low_hit.position + forward * (depth + VAULT_LANDING_DISTANCE) + tangent * lateral_offset;
    const auto ground_hit = probe(glm::vec3(landing.x, top_y + 0.3f, landing.z), glm::vec3(landing.x, top_y - 3.0f, landing.z));
    if (!ground_hit.hit)
    {
        return {};
    }

    TraversalPlan plan{};
    plan.type = TraversalPlan::Type::eVault;
    plan.wall_normal = wall_normal;
    plan.move.path.p0 = position;
    plan.move.path.p1 = glm::vec3(middle.x, top_y + VAULT_CLEARANCE, middle.z) + tangent * lateral_offset;
    plan.move.path.p2 = glm::vec3(landing.x, ground_hit.position.y + 0.01f, landing.z);
    plan.move.path.curved = true;
    plan.move.yaw = yaw_from_direction(forward);
    // on top of the sill
    plan.marker = glm::vec3(middle.x, top_y + TRAVERSAL_MARKER_SIZE, middle.z) + tangent * lateral_offset;
    return plan;
}

std::optional<project_c::PlayerTraversal::Edge> project_c::PlayerTraversal::find_edge(const glm::vec3& position, const glm::vec3& direction) const
{
    // sample a few points ahead, first one without floor below is the edge
    for (const float edge_distance : EDGE_SAMPLE_DISTANCES)
    {
        const glm::vec3 edge_point = position + direction * edge_distance;
        if (probe(edge_point + WORLD_UP * 0.2f, edge_point - WORLD_UP * 0.5f).hit)
        {
            continue;
        }
        const auto ground_hit = probe(edge_point - WORLD_UP * 0.5f, edge_point - WORLD_UP * EDGE_DROP_PROBE_DEPTH);
        return Edge{ edge_distance, ground_hit.hit ? position.y - ground_hit.position.y : std::numeric_limits<float>::infinity() };
    }
    return std::nullopt;
}

project_c::PlayerTraversal::TraversalPlan project_c::PlayerTraversal::plan_climb_down(const glm::vec3& position, const glm::vec3& direction) const
{
    const auto edge = find_edge(position, direction);
    if (!edge)
    {
        return {};
    }

    // look back at the wall below the edge
    const glm::vec3 below = -WORLD_UP * (PLAYER_HEIGHT * 0.7f);
    const auto wall_hit = probe(position + direction * (edge->distance + CLIMB_DOWN_EDGE_DISTANCE) + below, position + below);
    if (!wall_hit.climbable)
    {
        return {};
    }
    const glm::vec3 wall_normal = flatten(wall_hit.normal);
    if (glm::dot(wall_normal, direction) < TRAVERSAL_MIN_APPROACH_DOT)
    {
        return {};
    }

    glm::vec3 hang_position = wall_hit.position + wall_normal * CLIMB_WALL_OFFSET;
    hang_position.y = position.y - CLIMB_DOWN_HANG_DEPTH;

    TraversalPlan plan{};
    plan.type = TraversalPlan::Type::eClimbDown;
    plan.distance = edge->distance - CLIMB_DOWN_EDGE_DISTANCE;
    plan.drop = edge->drop;
    plan.wall_normal = wall_normal;
    plan.move.path.p0 = position;
    plan.move.path.p1 = glm::vec3(hang_position.x, position.y + 0.1f, hang_position.z);
    plan.move.path.p2 = hang_position;
    plan.move.path.split = 0.4f;
    plan.move.yaw = yaw_from_direction(-wall_normal);
    plan.move.climb_after = true;
    // on the edge
    plan.marker = glm::vec3(wall_hit.position.x, position.y + TRAVERSAL_MARKER_SIZE, wall_hit.position.z) - wall_normal * TRAVERSAL_MARKER_SIZE;
    return plan;
}

project_c::MovementMode project_c::PlayerTraversal::start_action(traversal_math::GroundAction action, const glm::vec3& position, const TraversalPlan& plan,
    const TraversalTuning& tuning)
{
    auto move = plan.move;
    switch (action)
    {
    case traversal_math::GroundAction::eRunUp:
        if (const auto run_up = plan_run_up(position, plan, tuning))
        {
            return start_run_up(run_up.value(), plan.wall_normal);
        }
        return start_climbing(plan.wall_normal, plan.climb_position);
    case traversal_math::GroundAction::eClimb:
        return start_climbing(plan.wall_normal, plan.climb_position);
    case traversal_math::GroundAction::eVault:
        move.duration = tuning.vault_duration_seconds;
        return start_move(move, VAULT_ANIMATION);
    case traversal_math::GroundAction::eClimbDown:
        climb_wall_normal_ = plan.wall_normal;
        move.duration = tuning.climb_down_duration_seconds;
        return start_move(move, CLIMB_ANIMATION);
    case traversal_math::GroundAction::eRunOff:
    case traversal_math::GroundAction::eNone:
        // walking on: past the edge the character controller lets the player fall
        break;
    }
    return MovementMode::eGrounded;
}

project_c::MovementMode project_c::PlayerTraversal::start_leap(const glm::vec3& position, const glm::vec3& direction, float drop, const TraversalTuning& tuning)
{
    // aimed at the ground below the edge; with none, at the takeoff's height
    // the gravity the character controller falls with
    float gravity[3] = {};
    enginePhysicsGetGravityVector(gravity);
    const auto launch = traversal_math::leap_launch(tuning.leap_distance, tuning.leap_height, std::isfinite(drop) ? drop : 0.0f, -gravity[1]);
    air_velocity_ = glm::vec2(direction.x, direction.z) * launch.horizontal_speed;
    launch_vertical_speed_ = launch.vertical_speed;
    set_position_and_yaw(position, yaw_from_direction(direction));

    engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, false);
    engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, 1.0f);
    engineAnimationControllerAnimationCrossFade(go_, LEAP_ANIMATION, locomotion_layer_, 0.05f);
    return MovementMode::eAirborne;
}

project_c::MovementMode project_c::PlayerTraversal::start_falling(const glm::vec3& velocity)
{
    approach_.reset();
    update_marker({}, 0.0f);
    air_velocity_ = glm::vec2(velocity.x, velocity.z);
    launch_vertical_speed_ = 0.0f;
    engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, true);
    engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, 1.0f);
    engineAnimationControllerAnimationCrossFade(go_, AIRBORNE_ANIMATION, locomotion_layer_, 0.15f);
    return MovementMode::eAirborne;
}

project_c::MovementMode project_c::PlayerTraversal::update_airborne(const glm::vec3& move_direction, float move_speed, float acceleration,
    const TraversalTuning& tuning, float delta_seconds)
{
    update_marker({}, delta_seconds);
    if (delta_seconds <= 0.0f)
    {
        return MovementMode::eAirborne;
    }
    air_velocity_ = traversal_math::air_control(air_velocity_, glm::vec2(move_direction.x, move_direction.z), move_speed,
        tuning.air_control * acceleration, delta_seconds);

    // the leap's launch keeps pushing up and the character controller adds the fall (its gravity): the arc
    const auto before = engineGameObjectGetTransformComponent(go_);
    const glm::vec3 velocity(air_velocity_.x, launch_vertical_speed_, air_velocity_.y);
    engine_character_controller_move_result_t result{};
    if (engineCharacterControllerMove(go_, glm::value_ptr(velocity), delta_seconds * 1000.0f, &result) != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to move the player in the air.\n");
        return MovementMode::eGrounded;
    }
    if (result.hit_ceiling)
    {
        launch_vertical_speed_ = 0.0f;
    }
    // what blocked it (a wall) took its speed
    const auto after = engineGameObjectGetTransformComponent(go_);
    air_velocity_ = glm::vec2(after.position[0] - before.position[0], after.position[2] - before.position[2]) / delta_seconds;

    // after the leap's takeoff, the pose in the air
    if (!engineAnimationControllerIsAnimationPlayingOnLayer(go_, LEAP_ANIMATION, locomotion_layer_)
        && !engineAnimationControllerIsAnimationPlayingOnLayer(go_, AIRBORNE_ANIMATION, locomotion_layer_))
    {
        engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, true);
        engineAnimationControllerAnimationCrossFade(go_, AIRBORNE_ANIMATION, locomotion_layer_, 0.1f);
    }
    // landed: on walkable ground, or held up by something which isn't (a too steep slope, an edge), where the player
    // walks on as before (it would never land there otherwise)
    return result.grounded || result.blocked_below ? MovementMode::eGrounded : MovementMode::eAirborne;
}

std::optional<project_c::traversal_math::MovePath> project_c::PlayerTraversal::plan_ledge_move(const glm::vec3& start, const glm::vec3& into_wall,
    float max_height, float split) const
{
    const glm::vec3 ledge_xz = start + into_wall * (CLIMB_WALL_OFFSET + MANTLE_LEDGE_DEPTH);
    const auto ledge_hit = probe(
        glm::vec3(ledge_xz.x, start.y + max_height, ledge_xz.z),
        glm::vec3(ledge_xz.x, start.y + PROBE_KNEE_HEIGHT, ledge_xz.z));
    if (!ledge_hit.building || ledge_hit.normal.y < 0.7f)
    {
        return std::nullopt;
    }

    // standing room above the ledge
    if (probe(ledge_hit.position + WORLD_UP * 0.02f, ledge_hit.position + WORLD_UP * PLAYER_HEIGHT).hit)
    {
        return std::nullopt;
    }

    // nothing above the player on the way up (i.e. ceiling when climbing on the inner side of a wall)
    const float ledge_y = ledge_hit.position.y;
    if (probe(start + WORLD_UP * PROBE_CHEST_HEIGHT, glm::vec3(start.x, ledge_y + PLAYER_HEIGHT, start.z)).hit)
    {
        return std::nullopt;
    }

    traversal_math::MovePath path{};
    path.p0 = start;
    path.p1 = glm::vec3(start.x, ledge_y + 0.05f, start.z);
    path.p2 = start + into_wall * (CLIMB_WALL_OFFSET + MANTLE_LEDGE_DEPTH + 0.15f);
    path.p2.y = ledge_y + 0.02f;
    path.split = split;
    return path;
}

bool project_c::PlayerTraversal::try_start_mantle(const glm::vec3& position, const TraversalTuning& tuning)
{
    const glm::vec3 into_wall = -climb_wall_normal_;
    const auto path = plan_ledge_move(position, into_wall, MANTLE_MAX_LEDGE_HEIGHT, 0.55f);
    if (!path)
    {
        return false;
    }

    TraversalMove move{};
    move.path = path.value();
    move.duration = tuning.mantle_duration_seconds;
    move.yaw = yaw_from_direction(into_wall);
    start_move(move, MANTLE_ANIMATION);
    return true;
}

std::optional<project_c::PlayerTraversal::TraversalMove> project_c::PlayerTraversal::plan_run_up(const glm::vec3& position, const TraversalPlan& plan,
    const TraversalTuning& tuning) const
{
    const glm::vec3 wall_normal = plan.wall_normal;
    const glm::vec3 into_wall = -wall_normal;
    const glm::vec3& start = plan.climb_position;

    TraversalMove move{};
    move.yaw = yaw_from_direction(into_wall);

    // the top of the wall within the run-up height: up the wall and over the edge onto the ledge, from where the player is
    if (const auto path = plan_ledge_move(start, into_wall, tuning.run_up_max_height - (start.y - position.y), RUN_UP_WALL_FRACTION))
    {
        move.path = path.value();
        move.path.p0 = position;
        move.duration = tuning.run_up_duration_seconds;
        move.finish_animation = RUN_UP_FINISH_ANIMATION;
        return move;
    }

    // higher: up to where the hands reach the run-up height, then climbing, while the wall goes on and nothing is above
    const glm::vec3 end(start.x, position.y + std::max(tuning.run_up_max_height - PLAYER_HEIGHT, 0.0f), start.z);
    const glm::vec3 grab = end + WORLD_UP * PROBE_HEAD_HEIGHT;
    if (!probe(grab, grab - wall_normal * (CLIMB_WALL_OFFSET + CLIMB_PROBE_DEPTH)).climbable
        || probe(start + WORLD_UP * PROBE_CHEST_HEIGHT, grab).hit)
    {
        return std::nullopt;
    }
    move.path.p0 = position;
    move.path.p1 = end;
    // where climbing starts (update_scripted_move)
    move.path.p2 = end;
    move.path.split = 1.0f;
    move.duration = tuning.run_up_duration_seconds * RUN_UP_WALL_FRACTION;
    move.climb_after = true;
    return move;
}

project_c::MovementMode project_c::PlayerTraversal::start_run_up(const TraversalMove& move, const glm::vec3& wall_normal)
{
    climb_wall_normal_ = wall_normal;
    move_ = move;
    move_.elapsed = 0.0f;
    move_.finish_started = false;
    // running up the wall; the jump over the edge starts with the move's second segment (update_scripted_move)
    engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, true);
    engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, RUN_UP_ANIMATION_RATE);
    if (!engineAnimationControllerIsAnimationPlayingOnLayer(go_, RUN_UP_ANIMATION, locomotion_layer_))
    {
        engineAnimationControllerAnimationCrossFade(go_, RUN_UP_ANIMATION, locomotion_layer_, 0.05f);
    }
    return MovementMode::eScriptedMove;
}

project_c::MovementMode project_c::PlayerTraversal::start_climbing(const glm::vec3& wall_normal, const glm::vec3& position)
{
    climb_wall_normal_ = wall_normal;
    set_position_and_yaw(position, yaw_from_direction(-wall_normal));

    engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, true);
    if (!engineAnimationControllerIsAnimationPlayingOnLayer(go_, CLIMB_ANIMATION, locomotion_layer_))
    {
        engineAnimationControllerAnimationCrossFade(go_, CLIMB_ANIMATION, locomotion_layer_, 0.08f);
    }
    return MovementMode::eClimbing;
}

project_c::MovementMode project_c::PlayerTraversal::start_move(const TraversalMove& move, const char* animation_name)
{
    move_ = move;
    move_.elapsed = 0.0f;

    // stretch the clip over the whole move
    float duration = 0.0f;
    engineAnimationControllerAnimationGetDuration(go_, animation_name, &duration);
    const float playback_rate = duration > 0.0f ? std::clamp(duration / move.duration, 0.25f, 4.0f) : 1.0f;
    engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, playback_rate);
    if (!engineAnimationControllerIsAnimationPlayingOnLayer(go_, animation_name, locomotion_layer_))
    {
        engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, false);
        engineAnimationControllerAnimationCrossFade(go_, animation_name, locomotion_layer_, 0.06f);
    }
    return MovementMode::eScriptedMove;
}

project_c::MovementMode project_c::PlayerTraversal::end(float regrab_cooldown)
{
    // player usually still holds the direction toward the wall, do not grab it again right away
    regrab_cooldown_ = regrab_cooldown;
    return MovementMode::eGrounded;
}

project_c::MovementMode project_c::PlayerTraversal::update_scripted_move(float delta_seconds)
{
    update_marker({}, delta_seconds);
    move_.elapsed = std::min(move_.elapsed + delta_seconds, move_.duration);
    const float t = move_.duration > 0.0f ? move_.elapsed / move_.duration : 1.0f;
    set_position_and_yaw(traversal_math::evaluate_move_path(move_.path, t), move_.yaw);
    if (move_.finish_animation && !move_.finish_started && t >= move_.path.split)
    {
        // the second segment's clip, stretched over it
        move_.finish_started = true;
        float duration = 0.0f;
        engineAnimationControllerAnimationGetDuration(go_, move_.finish_animation, &duration);
        const float segment_seconds = move_.duration * (1.0f - move_.path.split);
        const float playback_rate = duration > 0.0f && segment_seconds > 0.0f ? std::clamp(duration / segment_seconds, 0.25f, 4.0f) : 1.0f;
        engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, false);
        engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, playback_rate);
        engineAnimationControllerAnimationCrossFade(go_, move_.finish_animation, locomotion_layer_, 0.05f);
    }

    if (move_.elapsed < move_.duration)
    {
        return MovementMode::eScriptedMove;
    }
    if (move_.climb_after)
    {
        return start_climbing(climb_wall_normal_, move_.path.p2);
    }
    return end(TRAVERSAL_REGRAB_COOLDOWN);
}

project_c::MovementMode project_c::PlayerTraversal::update_climbing(const glm::vec3& move_input, bool drop_pressed, const TraversalTuning& tuning,
    float delta_seconds)
{
    update_marker({}, delta_seconds);
    const auto tc = engineGameObjectGetTransformComponent(go_);
    glm::vec3 position(tc.position[0], tc.position[1], tc.position[2]);

    const glm::vec3 wall_normal = climb_wall_normal_;
    const glm::vec3 tangent = glm::normalize(glm::cross(WORLD_UP, wall_normal));  // character's right while facing the wall
    if (drop_pressed)
    {
        set_position_and_yaw(position + wall_normal * CLIMB_DROP_PUSH, yaw_from_direction(-wall_normal));
        return end(TRAVERSAL_DROP_REGRAB_COOLDOWN);
    }

    const auto wall_probe = [&](const glm::vec3& feet, float height)
        {
            const glm::vec3 from = feet + WORLD_UP * height;
            return probe(from, from - wall_normal * (CLIMB_WALL_OFFSET + CLIMB_PROBE_DEPTH));
        };
    const auto touches_wall = [&](const glm::vec3& feet)
        {
            for (const float height : { PROBE_KNEE_HEIGHT, PROBE_CHEST_HEIGHT, PROBE_HEAD_HEIGHT })
            {
                if (wall_probe(feet, height).climbable)
                {
                    return true;
                }
            }
            return false;
        };

    // Same WASD directions as on the ground: pushing toward the wall climbs up, away from it climbs down,
    // along the wall shimmies sideways. I.e. wall on the right side of the screen -> holding D climbs.
    float vertical_input = glm::dot(move_input, -wall_normal);
    float lateral_input = glm::dot(move_input, tangent);
    if (std::abs(vertical_input) < CLIMB_INPUT_DEADZONE) vertical_input = 0.0f;
    if (std::abs(lateral_input) < CLIMB_INPUT_DEADZONE) lateral_input = 0.0f;

    if (lateral_input != 0.0f)
    {
        const float lateral_sign = lateral_input > 0.0f ? 1.0f : -1.0f;
        const glm::vec3 candidate = position + tangent * (lateral_input * tuning.climb_speed_lateral * delta_seconds);
        // leading side of the body has to stay on the wall, so the player does not hang over a corner
        if (touches_wall(candidate + tangent * (lateral_sign * PLAYER_HALF_WIDTH)))
        {
            position = candidate;
        }
    }

    if (vertical_input > 0.0f)
    {
        const glm::vec3 candidate = position + WORLD_UP * (vertical_input * tuning.climb_speed_vertical * delta_seconds);
        if (!wall_probe(candidate, PROBE_HEAD_HEIGHT).climbable)
        {
            // head above the wall: pull up onto the ledge if there is one
            if (try_start_mantle(position, tuning))
            {
                return MovementMode::eScriptedMove;
            }
            if (touches_wall(candidate))
            {
                position = candidate;
            }
        }
        else
        {
            position = candidate;
        }
    }
    else if (vertical_input < 0.0f)
    {
        const glm::vec3 candidate = position + WORLD_UP * (vertical_input * tuning.climb_speed_vertical * delta_seconds);
        const auto ground_hit = probe(position + WORLD_UP * 0.05f, candidate - WORLD_UP * 0.02f);
        if (ground_hit.hit && ground_hit.normal.y > 0.7f)
        {
            // reached the ground
            set_position_and_yaw(glm::vec3(position.x, ground_hit.position.y + 0.01f, position.z), yaw_from_direction(-wall_normal));
            return end(TRAVERSAL_REGRAB_COOLDOWN);
        }
        if (touches_wall(candidate))
        {
            position = candidate;
        }
    }

    // keep constant distance from the wall surface
    bool on_wall = false;
    for (const float height : { PROBE_CHEST_HEIGHT, PROBE_KNEE_HEIGHT, PROBE_HEAD_HEIGHT })
    {
        const auto hit = wall_probe(position, height);
        if (hit.climbable)
        {
            position += wall_normal * (CLIMB_WALL_OFFSET - glm::dot(position - hit.position, wall_normal));
            on_wall = true;
            break;
        }
    }
    set_position_and_yaw(position, yaw_from_direction(-wall_normal));
    if (!on_wall)
    {
        // lost the wall (i.e. it was removed), fall
        return end(TRAVERSAL_REGRAB_COOLDOWN);
    }

    const bool moving = vertical_input != 0.0f || lateral_input != 0.0f;
    engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, moving ? CLIMB_ANIMATION_RATE_MOVING : CLIMB_ANIMATION_RATE_IDLE);
    return MovementMode::eClimbing;
}

void project_c::PlayerTraversal::create_marker()
{
    auto* app = static_cast<AppProjectC*>(scene_->get_app());
    if (!app->is_prefab_available(PREFAB_TYPE_TRAVERSAL_MARKER))
    {
        return;
    }
    marker_ = app->spawn_prefab(PREFAB_TYPE_TRAVERSAL_MARKER).go;
    if (marker_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }

    auto tc = engineGameObjectGetTransformComponent(marker_);
    tc.scale[0] = TRAVERSAL_MARKER_SIZE;
    tc.scale[1] = TRAVERSAL_MARKER_SIZE;
    tc.scale[2] = TRAVERSAL_MARKER_SIZE;
    engineGameObjectUpdateTransformComponent(marker_, &tc);

    // flat color, set per traversal type
    auto mat = engineGameObjectHasMaterialComponent(marker_)
        ? engineGameObjectGetMaterialComponent(marker_)
        : engineGameObjectAddMaterialComponent(marker_);
    mat.diffuse_texture = ENGINE_INVALID_OBJECT_HANDLE;
    mat.specular_texture = ENGINE_INVALID_OBJECT_HANDLE;
    engineGameObjectUpdateMaterialComponent(marker_, &mat);

    auto mc = engineGameObjectGetMeshComponent(marker_);
    mc.disable = true;
    engineGameObjectUpdateMeshComponent(marker_, &mc);
}

void project_c::PlayerTraversal::update_marker(const TraversalPlan& plan, float delta_seconds)
{
    if (marker_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }

    const bool visible = plan.type != TraversalPlan::Type::eNone;
    auto mc = engineGameObjectGetMeshComponent(marker_);
    if (mc.disable == visible)
    {
        mc.disable = !visible;
        engineGameObjectUpdateMeshComponent(marker_, &mc);
    }
    if (!visible)
    {
        marker_type_ = TraversalPlan::Type::eNone;
        marker_time_ = 0.0f;
        return;
    }

    if (marker_type_ != plan.type)
    {
        marker_type_ = plan.type;
        // climb - yellow, vault - green, climb down - cyan
        std::array<float, 4> color = { 1.0f, 0.85f, 0.1f, 1.0f };
        if (plan.type == TraversalPlan::Type::eVault)
        {
            color = { 0.2f, 1.0f, 0.3f, 1.0f };
        }
        else if (plan.type == TraversalPlan::Type::eClimbDown)
        {
            color = { 0.2f, 0.8f, 1.0f, 1.0f };
        }
        auto mat = engineGameObjectGetMaterialComponent(marker_);
        set_c_array(mat.diffuse_color, color);
        engineGameObjectUpdateMaterialComponent(marker_, &mat);
    }

    // pulse
    marker_time_ += delta_seconds;
    const float scale = TRAVERSAL_MARKER_SIZE * (1.0f + 0.3f * std::sin(marker_time_ * 10.0f));
    auto tc = engineGameObjectGetTransformComponent(marker_);
    tc.position[0] = plan.marker.x;
    tc.position[1] = plan.marker.y - scale;  // mesh pivot is at the bottom of the cube
    tc.position[2] = plan.marker.z;
    tc.scale[0] = scale;
    tc.scale[1] = scale;
    tc.scale[2] = scale;
    engineGameObjectUpdateTransformComponent(marker_, &tc);
}

void project_c::PlayerTraversal::set_position_and_yaw(const glm::vec3& position, float yaw)
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = position.x;
    tc.position[1] = position.y;
    tc.position[2] = position.z;
    const glm::quat rotation = glm::angleAxis(yaw, WORLD_UP);
    std::memcpy(tc.rotation, glm::value_ptr(rotation), sizeof(tc.rotation));
    engineGameObjectUpdateTransformComponent(go_, &tc);
}
