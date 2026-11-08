#include "player_shove.h"
#include "enemy_script.h"
#include "game_layers.h"

#include "iscene.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
// the most characters the search looks at (the nearest are kept)
constexpr std::size_t MAX_CHARACTERS_AROUND = 16;
// the search sphere's center above the feet: the capsules of characters on the same floor overlap it
constexpr float SEARCH_HEIGHT = 0.4f;
// an enemy this far above or below the player is on another level: not kicked
constexpr float MAX_HEIGHT_DIFFERENCE = 0.5f;

glm::vec3 position_of(engine_game_object_t go)
{
    const auto tc = engineGameObjectGetTransformComponent(go);
    return glm::vec3(tc.position[0], tc.position[1], tc.position[2]);
}
}  // namespace

project_c::PlayerShove::PlayerShove(engine::IScene* scene, engine_game_object_t player)
    : scene_(scene)
    , go_(player)
{
}

engine_game_object_t project_c::PlayerShove::find_target(const glm::vec3& position, const glm::vec3& forward, const PlayerTuning& tuning) const
{
    const glm::vec2 facing(forward.x, forward.z);
    if (glm::length(facing) <= 0.0f)
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    const glm::vec2 facing_direction = glm::normalize(facing);
    const float min_cos = std::cos(glm::radians(tuning.shove_half_angle_degrees));

    // the characters around (not the floor and walls): an overlap with the characters' collision layer
    engine_physics_query_filter_t filter{};
    filter.ignore = &go_;
    filter.ignore_count = 1;
    filter.layer_mask = layer_bit(Layer::eCharacter);
    std::array<engine_game_object_t, MAX_CHARACTERS_AROUND> found{};
    const glm::vec3 center = position + glm::vec3(0.0f, SEARCH_HEIGHT, 0.0f);
    const auto count = std::min(enginePhysicsOverlapSphere(glm::value_ptr(center), tuning.shove_range, &filter, found.data(), found.size()), found.size());

    engine_game_object_t best = ENGINE_INVALID_GAME_OBJECT_ID;
    float best_distance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < count; i++)
    {
        const auto* enemy = scene_->get_script<Enemy>(found[i]);
        if (!enemy || enemy->is_dying() || enemy->is_being_executed())
        {
            continue;
        }
        const auto enemy_position = position_of(found[i]);
        if (std::abs(enemy_position.y - position.y) > MAX_HEIGHT_DIFFERENCE)
        {
            continue;
        }
        const glm::vec2 offset(enemy_position.x - position.x, enemy_position.z - position.z);
        const float distance = glm::length(offset);
        if (distance > tuning.shove_range || distance >= best_distance)
        {
            continue;
        }
        // in front: within the half angle of the facing (one standing right on the player counts)
        if (distance > 0.0f && glm::dot(offset / distance, facing_direction) < min_cos)
        {
            continue;
        }
        best = found[i];
        best_distance = distance;
    }
    return best;
}

bool project_c::PlayerShove::contact(const glm::vec3& direction, const PlayerTuning& tuning)
{
    auto* enemy = scene_->get_script<Enemy>(target_);
    target_ = ENGINE_INVALID_GAME_OBJECT_ID;
    const glm::vec2 flat(direction.x, direction.z);
    if (!enemy || enemy->is_dying() || enemy->is_being_executed() || glm::length(flat) <= 0.0f)
    {
        return false;
    }
    const glm::vec2 push = glm::normalize(flat);
    enemy->receive_shove(glm::vec3(push.x, 0.0f, push.y), tuning.shove_speed, tuning.shove_stagger_seconds, tuning.shove_damage);
    return true;
}
