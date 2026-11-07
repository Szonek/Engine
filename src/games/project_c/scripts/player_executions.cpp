#include "player_executions.h"
#include "enemy_script.h"
#include "scripts_utils.h"

#include "iscene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
// where the player strikes from: this far behind the victim, facing its back
constexpr float EXECUTION_SPOT_OFFSET = 0.6f;
// the chest, above the feet: the line between the player and its target must be free of level geometry
constexpr float CHEST_HEIGHT = 0.5f;
// the height difference the ground execution and the chain allow
constexpr float EXECUTION_MAX_HEIGHT_DIFFERENCE = 0.5f;
// the shortest dash of the chain
constexpr float CHAIN_DASH_MIN_SECONDS = 0.05f;
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

project_c::PlayerExecutions::PlayerExecutions(engine::IScene* scene, engine_game_object_t player, std::size_t locomotion_layer)
    : scene_(scene)
    , go_(player)
    , locomotion_layer_(locomotion_layer)
{
}

project_c::PlayerExecutions::Target project_c::PlayerExecutions::pick(const glm::vec3& center, float range, const execution_math::Rules& rules,
    const glm::vec3& from) const
{
    // the enemies themselves, not an overlap query around the player: it would find the floor and walls first (nearest
    // first), which would push the enemies out of any buffer
    std::vector<execution_math::Candidate> candidates;
    std::vector<Target> targets;
    for (auto* enemy : scene_->get_scripts<Enemy>())
    {
        const auto go = enemy->get_game_object();
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
        const std::array<engine_game_object_t, 2> ignore = { go_, go };
        candidate.reachable = !utils::level_geometry_between(from + WORLD_UP * CHEST_HEIGHT, enemy_position + WORLD_UP * CHEST_HEIGHT, ignore);
        candidates.push_back(candidate);
        targets.push_back(Target{ go, enemy });
    }
    const auto best = execution_math::pick_target(candidates, rules);
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

project_c::MovementMode project_c::PlayerExecutions::try_ground(const Context& context, const ExecutionTuning& tuning)
{
    // the chain's target while its window is open; the press ends the window either way
    const bool chain = chain_window_.is_open();
    const auto go = find_ground_target(context, tuning);
    chain_window_.close();
    auto* enemy = scene_->get_script<Enemy>(go);
    if (!enemy)
    {
        // a normal attack: the chain ends
        return MovementMode::eGrounded;
    }
    const auto spot = execution_math::execution_spot(position_of(go), forward_of(go), EXECUTION_SPOT_OFFSET);
    const float move_seconds = chain
        ? execution_math::move_seconds(glm::distance(context.position, spot), tuning.chain_dash_speed, CHAIN_DASH_MIN_SECONDS)
        : tuning.execution_lunge_seconds;
    return start(Target{ go, enemy }, context, move_seconds, tuning);
}

project_c::MovementMode project_c::PlayerExecutions::start(const Target& target, const Context& context, float move_seconds, const ExecutionTuning& tuning)
{
    target.enemy->begin_execution();
    victim_ = target.go;
    const auto victim_forward = forward_of(target.go);
    move_from_ = context.position;
    move_to_ = execution_math::execution_spot(position_of(target.go), victim_forward, EXECUTION_SPOT_OFFSET);
    // facing its back
    yaw_ = std::atan2(victim_forward.x, victim_forward.z);
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
