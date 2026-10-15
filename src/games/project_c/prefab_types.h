#pragma once

namespace project_c
{
enum PrefabType
{
    PREFAB_TYPE_ROGUE,

    PREFAB_TYPE_SKELETON_WARRIOR,
    PREFAB_TYPE_SKELETON_BLADE,


    PREFAB_TYPE_FLOOR,
    PREFAB_FLOOR_OUTSIDE_REGION,
    PREFAB_TYPE_WALL,
    PREFAB_TYPE_BOX,
    PREFAB_TYPE_COIN_GOLD,

    // building pieces (climbable house)
    PREFAB_TYPE_BUILDING_WALL,
    PREFAB_TYPE_BUILDING_WINDOW,
    PREFAB_TYPE_BUILDING_ROOF,

    // player traversal indicator
    PREFAB_TYPE_TRAVERSAL_MARKER,

    PREFAB_TYPE_COUNT
};

} // namespace project_c