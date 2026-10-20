#include "level_builder.h"

#include "../app.h"
#include "../scripts/building_script.h"
#include "../scripts/enemy_script.h"
#include "../scripts/enviorment_script.h"
#include "../scripts/interactable_script.h"
#include "../scripts/player_script.h"

#include <iscene.h>

#include <cstring>

project_c::LevelBuilder::LevelBuilder(AppProjectC& app, engine::IScene& scene)
    : app_(app)
    , scene_(scene)
{
}

engine_game_object_t project_c::LevelBuilder::nav_surface()
{
    const auto go = engineGameObjectCreate();
    auto nc = engineGameObjectAddNameComponent(go);
    std::strncpy(nc.name, "nav-surface", ENGINE_ENTITY_NAME_MAX_LENGTH);
    engineGameObjectUpdateNameComponent(go, &nc);
    // the engine's defaults fit the skeletons
    engineGameObjectAddNavSurfaceComponent(go);
    return go;
}

project_c::LevelBuilder& project_c::LevelBuilder::floor(float x, float z)
{
    if (app_.is_prefab_available(PREFAB_TYPE_FLOOR))
    {
        scene_.register_script<Floor>(app_.instantiate_prefab(PREFAB_TYPE_FLOOR).go, x, z);
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

project_c::LevelBuilder& project_c::LevelBuilder::floor_not_walkable(float x, float z)
{
    if (app_.is_prefab_available(PREFAB_TYPE_FLOOR))
    {
        const auto go = app_.instantiate_prefab(PREFAB_TYPE_FLOOR).go;
        scene_.register_script<Floor>(go, x, z);
        auto nmc = engineGameObjectAddNavModifierComponent(go);
        nmc.type = ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE;
        engineGameObjectUpdateNavModifierComponent(go, &nmc);
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
    return player;
}

project_c::Enemy* project_c::LevelBuilder::enemy(float x, float z)
{
    if (!app_.is_prefab_available(PREFAB_TYPE_SKELETON_WARRIOR))
    {
        return nullptr;
    }
    return scene_.register_script<Enemy>(app_.instantiate_prefab(PREFAB_TYPE_SKELETON_WARRIOR), x, z);
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
    auto* weapon = scene_.register_script<Weapon>(engineGameObjectCreate());
    weapon->drop_on_ground(glm::vec3(x, 0.5f, z));
    return weapon;
}

project_c::House* project_c::LevelBuilder::house(float x, float z, float ground_y)
{
    return build_house(scene_, app_, glm::vec2(x, z), ground_y);
}
