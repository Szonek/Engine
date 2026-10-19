#include "level_builder.h"

#include "../app.h"
#include "../scripts/enemy_script.h"
#include "../scripts/enviorment_script.h"
#include "../scripts/interactable_script.h"
#include "../scripts/player_script.h"

#include <iscene.h>

#include <array>
#include <cmath>

namespace
{
std::pair<std::int32_t, std::int32_t> to_cell(float x, float z)
{
    return { static_cast<std::int32_t>(std::lround(x)), static_cast<std::int32_t>(std::lround(z)) };
}
}  // namespace

project_c::LevelBuilder::LevelBuilder(AppProjectC& app, engine::IScene& scene, NavMesh& nav_mesh)
    : app_(app)
    , scene_(scene)
    , nav_mesh_(nav_mesh)
{
}

project_c::LevelBuilder& project_c::LevelBuilder::floor(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_FLOOR))
    {
        return *this;
    }
    scene_.register_script<Floor>(app_.instantiate_prefab(PREFAB_TYPE_FLOOR).go, x, z);

    const auto cell = to_cell(x, z);
    if (nav_nodes_.contains(cell))
    {
        return *this;
    }
    const auto id = nav_mesh_.add_node({ x, 0.0f, z }, { 0.5f, 0.0f, 0.5f });
    nav_nodes_[cell] = id;

    // edge costs are integers, so a diagonal step costs the same as a straight one
    constexpr std::array<std::array<std::int32_t, 2>, 8> neighbours{ {
        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } } };
    for (const auto& [dx, dz] : neighbours)
    {
        const auto it = nav_nodes_.find({ cell.first + dx, cell.second + dz });
        if (it == nav_nodes_.end())
        {
            continue;
        }
        const auto cost = static_cast<NavMeshEdgeCost>(dx != 0 && dz != 0 ? 1.414f : 1.0f);
        nav_mesh_.add_edge(id, it->second, cost);
        nav_mesh_.add_edge(it->second, id, cost);
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor_area(float x_min, float z_min, float x_max, float z_max)
{
    for (auto x = x_min; x <= x_max; x += 1.0f)
    {
        for (auto z = z_min; z <= z_max; z += 1.0f)
        {
            floor(x, z);
        }
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor_without_nav(float x, float z)
{
    if (app_.is_prefab_available(PREFAB_TYPE_FLOOR))
    {
        scene_.register_script<Floor>(app_.instantiate_prefab(PREFAB_TYPE_FLOOR).go, x, z);
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor_outside_region(float x, float z)
{
    if (app_.is_prefab_available(PREFAB_FLOOR_OUTSIDE_REGION))
    {
        auto sc = scene_.register_script<FloorOutsideRegion>(app_.instantiate_prefab(PREFAB_FLOOR_OUTSIDE_REGION).go);
        sc->set_world_position(x, 0.0f, z);
    }
    return *this;
}

project_c::LevelBuilder& project_c::LevelBuilder::wall(float x, float z, float y_rotation_degrees)
{
    if (app_.is_prefab_available(PREFAB_TYPE_WALL))
    {
        scene_.register_script<Wall>(app_.instantiate_prefab(PREFAB_TYPE_WALL).go, x, z, y_rotation_degrees);
    }
    return *this;
}

engine_game_object_t project_c::LevelBuilder::obstacle(const glm::vec3& center, const glm::vec3& size)
{
    const auto go = engineGameObjectCreate();

    auto nc = engineGameObjectAddNameComponent(go);
    std::strncpy(nc.name, "obstacle", ENGINE_ENTITY_NAME_MAX_LENGTH);
    engineGameObjectUpdateNameComponent(go, &nc);

    auto tc = engineGameObjectAddTransformComponent(go);
    tc.position[0] = center.x;
    tc.position[1] = center.y;
    tc.position[2] = center.z;
    engineGameObjectUpdateTransformComponent(go, &tc);

    // no rigid body: a static collider
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    // box sizes are half extents
    cc.collider.box.size[0] = size.x * 0.5f;
    cc.collider.box.size[1] = size.y * 0.5f;
    cc.collider.box.size[2] = size.z * 0.5f;
    engineGameObjectUpdateColliderComponent(go, &cc);
    return go;
}

project_c::Player* project_c::LevelBuilder::player(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_ROGUE))
    {
        return nullptr;
    }
    auto* player = scene_.register_script<Player>(app_.instantiate_prefab(PREFAB_TYPE_ROGUE));
    player->set_world_position(x, 1.0f, z);
    auto* weapon = scene_.register_script<Weapon>();
    player->equip_waepon(weapon);
    return player;
}

project_c::Enemy* project_c::LevelBuilder::enemy(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_SKELETON_WARRIOR))
    {
        return nullptr;
    }
    return scene_.register_script<Enemy>(app_.instantiate_prefab(PREFAB_TYPE_SKELETON_WARRIOR), &nav_mesh_, x, z);
}

project_c::Chest* project_c::LevelBuilder::chest(float x, float y, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_BOX))
    {
        return nullptr;
    }
    auto* chest = scene_.register_script<Chest>(app_.instantiate_prefab(PREFAB_TYPE_BOX).go);
    chest->set_world_position(x, y, z);
    return chest;
}

project_c::Weapon* project_c::LevelBuilder::weapon_on_ground(float x, float z)
{
    // weapons reuse geometry and texture of the rogue asset
    if (!app_.is_prefab_available(PREFAB_TYPE_ROGUE))
    {
        return nullptr;
    }
    auto* weapon = scene_.register_script<Weapon>();
    weapon->drop_on_ground(glm::vec3(x, 0.5f, z));
    return weapon;
}
