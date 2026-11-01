#include "gizmos.h"

#include "game_components.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>

namespace
{
// the spawner's area is flat: drawn as a box this high, standing on the spawner's origin
constexpr float SPAWNER_AREA_HEIGHT = 0.5f;
constexpr float PLAYER_START_ARROW_LENGTH = 0.5f;
constexpr float PLAYER_START_RADIUS = 0.2f;

glm::vec3 gizmo_color(const glm::vec3& color, bool selected)
{
    return selected ? color : color * 0.55f;
}

std::optional<glm::mat4> world_matrix(engine_game_object_t go)
{
    float world[16] = {};
    if (engineGameObjectGetWorldTransform(go, world) != ENGINE_RESULT_CODE_OK)
    {
        return std::nullopt;
    }
    return glm::make_mat4(world);
}

void draw_enemy_spawner(engine_game_object_t go, bool selected)
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

void draw_player_start(engine_game_object_t go, bool selected)
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
    engineDebugDrawCircle(glm::value_ptr(origin), glm::value_ptr(up), PLAYER_START_RADIUS, glm::value_ptr(color));
    engineDebugDrawLine(glm::value_ptr(origin), glm::value_ptr(tip), glm::value_ptr(color));
    for (const float side : { -1.0f, 1.0f })
    {
        const auto barb = tip - forward * head + right * (side * head * 0.6f);
        engineDebugDrawLine(glm::value_ptr(tip), glm::value_ptr(barb), glm::value_ptr(color));
    }
}
}  // namespace

void project_c::register_gizmos(engine::GizmoRegistry& gizmos)
{
    gizmos.add(game_components().enemy_spawner, &draw_enemy_spawner);
    gizmos.add(game_components().player_start, &draw_player_start);
}
