#include "player_smoke.h"
#include "scripts_utils.h"
#include "smoke_cloud.h"

#include "iscene.h"

#include <algorithm>

namespace
{
// The ground under the landing spot is looked for this far down from just above the cursor point when the throw reaches
// it (the cursor ray found that surface already: a roof under the cursor puts the cloud on the roof, the floor under an
// overhang on the floor), and from this far above the higher of the player and the cursor point when the landing was
// clamped to the range (nothing is known about the ground there).
constexpr float GROUND_PROBE_ABOVE_CURSOR = 0.1f;
constexpr float GROUND_PROBE_ABOVE = 5.0f;
constexpr float GROUND_PROBE_DEPTH = 50.0f;
}  // namespace

project_c::PlayerSmoke::PlayerSmoke(engine::IScene* scene, engine_game_object_t player)
    : scene_(scene)
    , go_(player)
{
}

void project_c::PlayerSmoke::update(float delta_seconds)
{
    cooldown_remaining_seconds_ = std::max(0.0f, cooldown_remaining_seconds_ - delta_seconds);
}

void project_c::PlayerSmoke::begin(const glm::vec3& player, const glm::vec3& cursor_point, const SmokeTuning& tuning)
{
    auto landing = smoke_landing(player, cursor_point, tuning.throw_range);
    const bool clamped = landing.x != cursor_point.x || landing.z != cursor_point.z;
    // on the ground there: the first level geometry below
    const float probe_y = clamped ? std::max(player.y, cursor_point.y) + GROUND_PROBE_ABOVE : cursor_point.y + GROUND_PROBE_ABOVE_CURSOR;
    const glm::vec3 probe(landing.x, probe_y, landing.z);
    if (const auto ground = utils::level_ray(probe, glm::vec3(0.0f, -1.0f, 0.0f), GROUND_PROBE_DEPTH))
    {
        landing.y = ground->position[1];
    }
    landing_ = landing;
    cooldown_remaining_seconds_ = tuning.cooldown_seconds;
}

bool project_c::PlayerSmoke::release(const SmokeTuning& tuning)
{
    if (!landing_)
    {
        return false;
    }
    // a runtime object (transient, never saved), like the coins
    const auto go = engineGameObjectCreateTransient();
    engineGameObjectAddTransformComponent(go);
    scene_->register_script<SmokeCloud>(go, *landing_, tuning);
    landing_.reset();
    return true;
}
