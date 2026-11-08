#include "gizmos.h"

#include "enemy_script.h"
#include "game_components.h"

#include <iscene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>

namespace
{
// the spawner's area is flat: drawn as a box this high, standing on the spawner's origin
constexpr float SPAWNER_AREA_HEIGHT = 0.5f;
constexpr float PLAYER_START_ARROW_LENGTH = 0.5f;
constexpr float PLAYER_START_RADIUS = 0.15f;
// the sight cone's arc is drawn in this many segments, a little above the enemy's feet
constexpr int SIGHT_ARC_SEGMENTS = 16;
constexpr float SIGHT_GIZMO_HEIGHT = 0.05f;

glm::vec3 gizmo_color(const glm::vec3& color, bool selected)
{
    return selected ? color : color * 0.55f;
}

// computed from the current transforms: drawers run before this frame's scene update, which draws the meshes where
// they are now (the world transform component is the last update's, a frame behind a gizmo drag)
std::optional<glm::mat4> world_matrix(engine_game_object_t go)
{
    float world[16] = {};
    if (engineGameObjectComputeWorldTransform(go, world) != ENGINE_RESULT_CODE_OK)
    {
        return std::nullopt;
    }
    return glm::make_mat4(world);
}

void draw_enemy_spawner(const engine::IScene&, engine_game_object_t go, bool selected)
{
    const auto world = world_matrix(go);
    if (!world)
    {
        return;
    }
    const auto cfg = project_c::get_game_component(go, project_c::game_components().enemy_spawner, project_c::default_enemy_spawner());
    const glm::vec3 half_extents(std::max(0.0f, cfg.area_half_extents[0]), SPAWNER_AREA_HEIGHT * 0.5f, std::max(0.0f, cfg.area_half_extents[1]));
    const auto box = glm::translate(*world, glm::vec3(0.0f, half_extents.y, 0.0f));
    const auto color = gizmo_color(glm::vec3(1.0f, 0.45f, 0.1f), selected);
    engineDebugDrawBox(glm::value_ptr(box), glm::value_ptr(half_extents), glm::value_ptr(color));
}

void draw_player_start(const engine::IScene&, engine_game_object_t go, bool selected)
{
    const auto world = world_matrix(go);
    if (!world)
    {
        return;
    }
    // the player is spawned facing the start's +z (LevelBuilder::spawn_player), whatever its scale
    const glm::vec3 origin((*world)[3]);
    auto forward = glm::vec3((*world)[2]);
    forward.y = 0.0f;
    if (glm::length(forward) <= 0.0f)
    {
        return;
    }
    forward = glm::normalize(forward);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const auto right = glm::cross(forward, up);
    const auto tip = origin + forward * PLAYER_START_ARROW_LENGTH;
    const auto head = PLAYER_START_ARROW_LENGTH * 0.3f;
    const auto color = gizmo_color(glm::vec3(0.2f, 1.0f, 0.3f), selected);
    // standing on the start's origin, where the player's feet go
    const auto sphere_center = origin + up * PLAYER_START_RADIUS;
    engineDebugDrawSphere(glm::value_ptr(sphere_center), PLAYER_START_RADIUS, glm::value_ptr(color));
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(tip), glm::value_ptr(color));
    for (const float side : { -1.0f, 1.0f })
    {
        const auto barb = tip - forward * head + right * (side * head * 0.6f);
        engineDebugDrawLine(glm::value_ptr(tip), glm::value_ptr(barb), glm::value_ptr(color));
    }
}
// the player's shove reach (FALLS_AND_SHOVES.md): the wedge in front of it an enemy is kicked in
void draw_player_shove(const engine::IScene&, engine_game_object_t go, bool selected)
{
    const auto world = world_matrix(go);
    if (!world)
    {
        return;
    }
    const auto& types = project_c::game_components();
    const auto config = project_c::get_game_component(go, types.player_config, project_c::default_player_config());
    const auto tuning = project_c::get_data_asset(config.tuning, types.player_tuning, project_c::default_player_tuning());
    auto forward = glm::vec3((*world)[2]);
    forward.y = 0.0f;
    if (glm::length(forward) <= 0.0f || tuning.shove_range <= 0.0f)
    {
        return;
    }
    forward = glm::normalize(forward);
    const glm::vec3 origin = glm::vec3((*world)[3]) + glm::vec3(0.0f, SIGHT_GIZMO_HEIGHT, 0.0f);
    const auto color = gizmo_color(glm::vec3(0.95f, 0.6f, 0.15f), selected);
    const float half_angle = glm::radians(tuning.shove_half_angle_degrees);
    const auto ray = [&](float angle)
        {
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            return origin + glm::vec3(forward.x * c + forward.z * s, 0.0f, -forward.x * s + forward.z * c) * tuning.shove_range;
        };
    auto previous = ray(-half_angle);
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(previous), glm::value_ptr(color));
    for (int i = 1; i <= SIGHT_ARC_SEGMENTS; i++)
    {
        const auto point = ray(-half_angle + 2.0f * half_angle * static_cast<float>(i) / static_cast<float>(SIGHT_ARC_SEGMENTS));
        engineDebugDrawLine(glm::value_ptr(previous), glm::value_ptr(point), glm::value_ptr(color));
        previous = point;
    }
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(previous), glm::value_ptr(color));
}

// the colour of an enemy's awareness: grey unaware (or without a script, in Edit mode), yellow suspicious, red alert,
// orange searching
glm::vec3 awareness_color(const project_c::Enemy* enemy)
{
    if (!enemy)
    {
        return glm::vec3(0.8f);
    }
    switch (enemy->awareness().state())
    {
    case project_c::Awareness::eSuspicious: return glm::vec3(1.0f, 0.9f, 0.1f);
    case project_c::Awareness::eAlert: return glm::vec3(1.0f, 0.15f, 0.1f);
    case project_c::Awareness::eSearching: return glm::vec3(1.0f, 0.55f, 0.1f);
    case project_c::Awareness::eUnaware: break;
    }
    return glm::vec3(0.6f);
}

// The perception of each enemy type, read from its data asset once per draw pass (the asset can change between passes:
// an edit, a reload), not per enemy.
class EnemyPerceptionCache
{
public:
    explicit EnemyPerceptionCache(const engine::GizmoRegistry& gizmos) : gizmos_(gizmos) {}

    const project_c::EnemyPerception& get(engine_data_asset_t type)
    {
        if (pass_ != gizmos_.pass())
        {
            pass_ = gizmos_.pass();
            perceptions_.clear();
        }
        auto it = perceptions_.find(type);
        if (it == perceptions_.end())
        {
            const auto& types = project_c::game_components();
            it = perceptions_.emplace(type, project_c::get_data_asset(type, types.enemy_type, project_c::default_enemy_type()).perception).first;
        }
        return it->second;
    }

private:
    const engine::GizmoRegistry& gizmos_;
    std::uint64_t pass_ = 0;
    std::unordered_map<engine_data_asset_t, project_c::EnemyPerception> perceptions_;
};

void draw_enemy(EnemyPerceptionCache& cache, const project_c::ExecutionTuning& executions, const engine::IScene& scene, engine_game_object_t go,
    bool selected)
{
    const auto world = world_matrix(go);
    if (!world)
    {
        return;
    }
    const auto& types = project_c::game_components();
    const auto config = project_c::get_game_component(go, types.enemy_config, project_c::default_enemy_config());
    const auto& perception = cache.get(config.type);
    // its facing, flat: +z of its game object
    auto forward = glm::vec3((*world)[2]);
    forward.y = 0.0f;
    if (glm::length(forward) <= 0.0f)
    {
        return;
    }
    forward = glm::normalize(forward);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 origin = glm::vec3((*world)[3]) + up * SIGHT_GIZMO_HEIGHT;
    const auto color = gizmo_color(awareness_color(scene.get_script<project_c::Enemy>(go)), selected);

    // the cone: its two edges and the arc between them at sight_range
    const float half_angle = glm::radians(perception.sight_half_angle_degrees);
    const auto ray = [&](float angle)
        {
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            // turned around +y from the facing
            return origin + glm::vec3(forward.x * c + forward.z * s, 0.0f, -forward.x * s + forward.z * c) * perception.sight_range;
        };
    auto previous = ray(-half_angle);
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(previous), glm::value_ptr(color));
    for (int i = 1; i <= SIGHT_ARC_SEGMENTS; i++)
    {
        const auto point = ray(-half_angle + 2.0f * half_angle * static_cast<float>(i) / static_cast<float>(SIGHT_ARC_SEGMENTS));
        engineDebugDrawLine(glm::value_ptr(previous), glm::value_ptr(point), glm::value_ptr(color));
        previous = point;
    }
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(previous), glm::value_ptr(color));
    // seen all around this close
    engineDebugDrawCircle(glm::value_ptr(origin), glm::value_ptr(up), perception.close_range, glm::value_ptr(color));

    // where the player executes it from: the arc behind it at the execution's range
    const auto execution_color = gizmo_color(glm::vec3(0.2f, 0.9f, 0.3f), selected);
    const float behind_half_angle = glm::radians(executions.execution_behind_half_angle_degrees);
    const auto behind = [&](float angle)
        {
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const glm::vec3 back = -forward;
            return origin + glm::vec3(back.x * c + back.z * s, 0.0f, -back.x * s + back.z * c) * executions.execution_range;
        };
    previous = behind(-behind_half_angle);
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(previous), glm::value_ptr(execution_color));
    for (int i = 1; i <= SIGHT_ARC_SEGMENTS; i++)
    {
        const auto point = behind(-behind_half_angle + 2.0f * behind_half_angle * static_cast<float>(i) / static_cast<float>(SIGHT_ARC_SEGMENTS));
        engineDebugDrawLine(glm::value_ptr(previous), glm::value_ptr(point), glm::value_ptr(execution_color));
        previous = point;
    }
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(previous), glm::value_ptr(execution_color));
}
}  // namespace

void project_c::register_gizmos(engine::GizmoRegistry& gizmos, std::function<ExecutionTuning()> execution_tuning)
{
    gizmos.add(game_components().enemy_spawner, &draw_enemy_spawner);
    gizmos.add(game_components().player_start, &draw_player_start);
    gizmos.add(game_components().player_config, &draw_player_shove);
    // the registry outlives its drawers (it holds them)
    gizmos.add(game_components().enemy_config, [cache = std::make_shared<EnemyPerceptionCache>(gizmos), execution_tuning = std::move(execution_tuning)](
        const engine::IScene& scene, engine_game_object_t go, bool selected)
        {
            draw_enemy(*cache, execution_tuning(), scene, go, selected);
        });
}
