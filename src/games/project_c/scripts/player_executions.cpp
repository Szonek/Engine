#include "player_executions.h"
#include "enemy_script.h"
#include "scripts_utils.h"
#include "game_layers.h"

#include "iscene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <optional>
#include <vector>

namespace
{
// where the player strikes from: this far behind the victim, facing its back
constexpr float EXECUTION_SPOT_OFFSET = 0.6f;
// the player's capsule: the strike spot keeps this far from walls
constexpr float PLAYER_RADIUS = 0.3f;
// the chest, above the feet: the line between the player and its target must be free of level geometry
constexpr float CHEST_HEIGHT = 0.5f;
// the height difference the ground execution and the chain allow
constexpr float EXECUTION_MAX_HEIGHT_DIFFERENCE = 0.5f;
// the shortest dash of the chain
constexpr float CHAIN_DASH_MIN_SECONDS = 0.05f;
// the shortest plunge
constexpr float PLUNGE_MIN_SECONDS = 0.12f;
// the plunge ends this far short of its target's position (flat), next to its body rather than in it
constexpr float PLUNGE_STOP_SHORT = 0.35f;
// the plunge knocks down the enemies around its impact on the same level only (not on a roof above it, nor below a
// ledge)
constexpr float KNOCKDOWN_MAX_HEIGHT_DIFFERENCE = 1.0f;
// how deep the ground below the player is looked for (its height above the ground, for aerial_min_drop)
constexpr float GROUND_PROBE_DEPTH = 50.0f;
// the clips, and when in them the strike lands (seconds of the clip)
constexpr const char* ARMED_CLIP = "Dualwield_Melee_Attack_Stab";
constexpr const char* UNARMED_CLIP = "Unarmed_Melee_Attack_Kick";
constexpr float ARMED_CLIP_HIT_SECONDS = 0.35f;
constexpr float UNARMED_CLIP_HIT_SECONDS = 0.3f;

const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);

glm::vec3 position_of(engine_game_object_t go)
{
    const auto tc = engineGameObjectGetTransformComponent(go);
    return glm::vec3(tc.position[0], tc.position[1], tc.position[2]);
}

glm::vec3 forward_of(engine_game_object_t go)
{
    const auto tc = engineGameObjectGetTransformComponent(go);
    return glm::make_quat(tc.rotation) * glm::vec3(0.0f, 0.0f, 1.0f);
}
}  // namespace

project_c::PlayerExecutions::CharactersAround project_c::PlayerExecutions::characters_within(const glm::vec3& center, float radius) const
{
    // the characters only (Layer::eCharacter), never the player itself
    engine_physics_query_filter_t filter{};
    filter.ignore = &go_;
    filter.ignore_count = 1;
    filter.layer_mask = layer_bit(Layer::eCharacter);
    CharactersAround around;
    const glm::vec3 sphere_center = center + WORLD_UP * CHEST_HEIGHT;
    const auto count = enginePhysicsOverlapSphere(glm::value_ptr(sphere_center), radius, &filter, around.found.data(), around.found.size());
    around.count = std::min(count, around.found.size());
    return around;
}

project_c::PlayerExecutions::PlayerExecutions(engine::IScene* scene, engine_game_object_t player, std::size_t locomotion_layer)
    : scene_(scene)
    , go_(player)
    , locomotion_layer_(locomotion_layer)
{
}

project_c::PlayerExecutions::Target project_c::PlayerExecutions::pick(const glm::vec3& center, float range, const execution_math::Rules& rules,
    const glm::vec3& from) const
{
    // the characters around: an overlap with the characters' layer (the floor and walls aren't on it); on the stack, this
    // runs every frame the player can execute
    std::array<execution_math::Candidate, MAX_CHARACTERS_AROUND> candidates{};
    std::array<Target, MAX_CHARACTERS_AROUND> targets{};
    std::size_t count = 0;
    for (const auto go : characters_within(center, range))
    {
        auto* enemy = scene_->get_script<Enemy>(go);
        if (!enemy)
        {
            continue;
        }
        const auto enemy_position = position_of(go);
        if (glm::distance(glm::vec2(center.x, center.z), glm::vec2(enemy_position.x, enemy_position.z)) > range)
        {
            continue;
        }
        execution_math::Candidate candidate{};
        candidate.awareness = enemy->awareness().state();
        candidate.sees_player = enemy->awareness().perceived_this_frame();
        candidate.knocked_down = enemy->is_knocked_down();
        candidate.unavailable = enemy->is_dying() || enemy->is_being_executed();
        candidate.distance = glm::distance(glm::vec2(from.x, from.z), glm::vec2(enemy_position.x, enemy_position.z));
        candidate.height_difference = from.y - enemy_position.y;
        candidate.angle_from_facing = execution_math::angle_from_facing(enemy_position, forward_of(go), from);
        // the ray cast only for the ones the other rules let through
        candidate.reachable = true;
        if (!execution_math::executable(candidate, rules))
        {
            continue;
        }
        candidate.reachable = !utils::level_blocks(from + WORLD_UP * CHEST_HEIGHT, enemy_position + WORLD_UP * CHEST_HEIGHT);
        candidates[count] = candidate;
        targets[count] = Target{ go, enemy };
        count++;
    }
    const auto best = execution_math::pick_target(std::span<const execution_math::Candidate>(candidates.data(), count), rules);
    return best ? targets[*best] : Target{};
}

engine_game_object_t project_c::PlayerExecutions::find_ground_target(const Context& context, const ExecutionTuning& tuning) const
{
    execution_math::Rules rules{};
    rules.max_height_difference = EXECUTION_MAX_HEIGHT_DIFFERENCE;
    if (chain_window_.is_open())
    {
        // the chain: any side, farther
        rules.range = tuning.chain_range;
    }
    else
    {
        rules.range = tuning.execution_range;
        rules.min_angle_from_facing = 180.0f - tuning.execution_behind_half_angle_degrees;
    }
    return pick(context.position, rules.range, rules, context.position).go;
}

project_c::MovementMode project_c::PlayerExecutions::try_ground(const Context& context, const ExecutionTuning& tuning, engine_game_object_t go)
{
    // the chain's target while its window is open; the press ends the window either way
    const bool chain = chain_window_.is_open();
    chain_window_.close();
    auto* enemy = scene_->get_script<Enemy>(go);
    if (!enemy)
    {
        // a normal attack: the chain ends
        return MovementMode::eGrounded;
    }
    const auto spot = strike_spot(go, context.position);
    const float move_seconds = chain
        ? execution_math::move_seconds(glm::distance(context.position, spot), tuning.chain_dash_speed, CHAIN_DASH_MIN_SECONDS)
        : tuning.execution_lunge_seconds;
    return start(Target{ go, enemy }, context, spot, move_seconds, tuning);
}

glm::vec3 project_c::PlayerExecutions::strike_spot(engine_game_object_t target, const glm::vec3& from) const
{
    const auto victim = position_of(target);
    const auto behind = execution_math::execution_spot(victim, forward_of(target), EXECUTION_SPOT_OFFSET);
    const auto chest = WORLD_UP * CHEST_HEIGHT;
    const auto to_behind = behind - victim;
    const auto margin = glm::length(to_behind) > 0.0f ? glm::normalize(to_behind) * PLAYER_RADIUS : glm::vec3(0.0f);
    if (!utils::level_blocks(victim + chest, behind + chest + margin))
    {
        return behind;
    }
    // a wall right behind it: from the side the player comes from, along the line between them, which is free of level
    // geometry (the target is reachable)
    glm::vec3 toward_player(from.x - victim.x, 0.0f, from.z - victim.z);
    const float distance = glm::length(toward_player);
    if (distance <= 0.0f)
    {
        return from;
    }
    return victim + toward_player / distance * std::min(EXECUTION_SPOT_OFFSET, distance);
}

void project_c::PlayerExecutions::observe_air(const glm::vec3& position, float delta_seconds)
{
    if (air_position_ && delta_seconds > 0.0f)
    {
        air_velocity_ = (position - *air_position_) / delta_seconds;
    }
    else if (!air_position_)
    {
        air_velocity_ = glm::vec3(0.0f);
    }
    air_position_ = position;
}

engine_game_object_t project_c::PlayerExecutions::find_plunge_target(const glm::vec3& position, const ExecutionTuning& tuning, float* knockdown_radius) const
{
    // high enough above the ground: level geometry, not the enemy right below (its capsule) nor a dropped weapon
    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    const auto ground = utils::level_ray(position, down, GROUND_PROBE_DEPTH);
    if (ground && position.y - ground->position[1] < tuning.aerial_min_drop)
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    float gravity[3] = {};
    enginePhysicsGetGravityVector(gravity);

    execution_math::Rules rules{};
    rules.range = tuning.aerial_aim_radius;
    rules.max_height_difference = std::numeric_limits<float>::infinity();
    rules.any_awareness = true;
    rules.allow_knocked_down = false;
    // where the fall comes down to each enemy below, and how far the enemy is from there
    std::vector<execution_math::Candidate> candidates;
    std::vector<engine_game_object_t> targets;
    std::vector<float> drops;
    for (auto* enemy : scene_->get_scripts<Enemy>())
    {
        const auto go = enemy->get_game_object();
        const auto enemy_position = position_of(go);
        const auto landing = execution_math::predict_landing(position, air_velocity_, -gravity[1], enemy_position.y);
        if (enemy_position.y >= position.y || !landing)
        {
            continue;
        }
        execution_math::Candidate candidate{};
        candidate.awareness = enemy->awareness().state();
        candidate.sees_player = enemy->awareness().perceived_this_frame();
        candidate.knocked_down = enemy->is_knocked_down();
        candidate.unavailable = enemy->is_dying() || enemy->is_being_executed();
        candidate.distance = glm::distance(glm::vec2(landing->point.x, landing->point.z), glm::vec2(enemy_position.x, enemy_position.z));
        // the plunge is a straight line: no roof or wall on the way (the ray only for the ones the other rules let
        // through)
        candidate.reachable = true;
        if (!execution_math::executable(candidate, rules))
        {
            continue;
        }
        candidate.reachable = !utils::level_blocks(position + WORLD_UP * CHEST_HEIGHT, enemy_position + WORLD_UP * CHEST_HEIGHT);
        candidates.push_back(candidate);
        targets.push_back(go);
        drops.push_back(position.y - enemy_position.y);
    }
    const auto best = execution_math::pick_target(candidates, rules);
    if (!best)
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    if (knockdown_radius)
    {
        *knockdown_radius = execution_math::knockdown_radius(drops[*best], tuning.knockdown_radius_per_metre, tuning.knockdown_max_radius);
    }
    return targets[*best];
}

project_c::MovementMode project_c::PlayerExecutions::try_plunge(const glm::vec3& position, const ExecutionTuning& tuning)
{
    float knockdown_radius = 0.0f;
    const auto go = find_plunge_target(position, tuning, &knockdown_radius);
    auto* enemy = scene_->get_script<Enemy>(go);
    if (!enemy)
    {
        return MovementMode::eAirborne;
    }
    enemy->begin_execution();
    victim_ = go;
    plunge_ = true;
    knockdown_radius_ = knockdown_radius;
    const auto target = position_of(go);
    glm::vec2 approach(target.x - position.x, target.z - position.z);
    const float approach_length = glm::length(approach);
    approach = approach_length > 0.0f ? approach / approach_length : glm::vec2(0.0f);
    move_from_ = position;
    move_to_ = target - glm::vec3(approach.x, 0.0f, approach.y) * std::min(PLUNGE_STOP_SHORT, approach_length);
    yaw_ = approach_length > 0.0f ? std::atan2(approach.x, approach.y) : 0.0f;
    elapsed_ = 0.0f;
    move_seconds_ = execution_math::move_seconds(glm::distance(move_from_, move_to_), tuning.plunge_speed, PLUNGE_MIN_SECONDS);
    struck_ = false;
    air_position_.reset();
    return MovementMode::eExecuting;
}

project_c::MovementMode project_c::PlayerExecutions::start(const Target& target, const Context& context, const glm::vec3& spot, float move_seconds,
    const ExecutionTuning& tuning)
{
    target.enemy->begin_execution();
    victim_ = target.go;
    move_from_ = context.position;
    move_to_ = spot;
    // facing the victim (its back, from the usual spot)
    const auto victim = position_of(target.go);
    const glm::vec2 facing(victim.x - spot.x, victim.z - spot.z);
    const auto victim_forward = forward_of(target.go);
    yaw_ = glm::length(facing) > 0.0f ? std::atan2(facing.x, facing.y) : std::atan2(victim_forward.x, victim_forward.z);
    elapsed_ = 0.0f;
    move_seconds_ = move_seconds;
    clip_ = context.armed ? ARMED_CLIP : UNARMED_CLIP;
    const float hit_seconds = context.armed ? ARMED_CLIP_HIT_SECONDS : UNARMED_CLIP_HIT_SECONDS;
    // the clip starts so that it hits once the player stands behind the victim
    clip_start_ = std::max(0.0f, move_seconds_ - hit_seconds);
    strike_at_ = clip_start_ + hit_seconds;
    end_at_ = clip_start_ + std::max(tuning.execution_seconds, hit_seconds);
    clip_started_ = false;
    struck_ = false;
    plunge_ = false;
    return MovementMode::eExecuting;
}

project_c::PlayerExecutions::Update project_c::PlayerExecutions::update(const ExecutionTuning& tuning, float delta_seconds)
{
    Update result{};
    elapsed_ += delta_seconds;
    auto* victim = scene_->get_script<Enemy>(victim_);
    if (!struck_ && !victim)
    {
        // its script went before the strike: nothing to execute
        victim_ = ENGINE_INVALID_GAME_OBJECT_ID;
        result.mode = MovementMode::eGrounded;
        return result;
    }

    const float t = move_seconds_ > 0.0f ? std::min(elapsed_ / move_seconds_, 1.0f) : 1.0f;
    if (plunge_)
    {
        // a straight line onto the victim, at the plunge's speed
        set_position_and_yaw(glm::mix(move_from_, move_to_, t), yaw_);
        if (t < 1.0f)
        {
            return result;
        }
        // the impact: the victim is executed, the others around it are knocked down (and alerted)
        victim->receive_execution(static_cast<std::uint32_t>(tuning.execution_damage));
        const auto impact = position_of(victim_);
        // the sphere reaches the characters up and down the height allowed too (it is centered at the impact's chest)
        const float search_radius = std::hypot(knockdown_radius_, KNOCKDOWN_MAX_HEIGHT_DIFFERENCE + CHEST_HEIGHT);
        for (const auto go : characters_within(impact, search_radius))
        {
            auto* other = scene_->get_script<Enemy>(go);
            if (!other)
            {
                continue;
            }
            const auto other_position = position_of(go);
            if (other != victim && !other->is_dying()
                && std::abs(other_position.y - impact.y) <= KNOCKDOWN_MAX_HEIGHT_DIFFERENCE
                && glm::distance(glm::vec2(impact.x, impact.z), glm::vec2(other_position.x, other_position.z)) <= knockdown_radius_)
            {
                other->knock_down(tuning.knockdown_seconds);
            }
        }
        victim_ = ENGINE_INVALID_GAME_OBJECT_ID;
        plunge_ = false;
        result.mode = MovementMode::eGrounded;
        result.plunged = true;
        return result;
    }
    set_position_and_yaw(glm::mix(move_from_, move_to_, glm::smoothstep(0.0f, 1.0f, t)), yaw_);

    if (!clip_started_ && elapsed_ >= clip_start_)
    {
        clip_started_ = true;
        engineAnimationControllerLayerSetLooping(go_, locomotion_layer_, false);
        engineAnimationControllerLayerSetPlaybackRate(go_, locomotion_layer_, 1.0f);
        engineAnimationControllerAnimationCrossFade(go_, clip_, locomotion_layer_, 0.05f);
    }
    if (!struck_ && elapsed_ >= strike_at_)
    {
        struck_ = true;
        result.struck = true;
        victim->receive_execution(static_cast<std::uint32_t>(tuning.execution_damage));
    }
    if (elapsed_ >= end_at_)
    {
        victim_ = ENGINE_INVALID_GAME_OBJECT_ID;
        chain_window_.open(tuning.chain_window_seconds);
        result.mode = MovementMode::eGrounded;
    }
    return result;
}

void project_c::PlayerExecutions::set_position_and_yaw(const glm::vec3& position, float yaw)
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = position.x;
    tc.position[1] = position.y;
    tc.position[2] = position.z;
    const glm::quat rotation = glm::angleAxis(yaw, WORLD_UP);
    std::memcpy(tc.rotation, glm::value_ptr(rotation), sizeof(tc.rotation));
    engineGameObjectUpdateTransformComponent(go_, &tc);
}
