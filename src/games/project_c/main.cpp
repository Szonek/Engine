#include <engine.h>

#include "app.h"

int main(int argc, char** argv)
{
    try
    {
        const std::unordered_map<project_c::PrefabType, std::pair<std::string, std::string>> prefabs_data =
        {
            //utils
            { project_c::PREFAB_TYPE_DEFAULT_CUBE,     { "default_cube.glb", "" }},
            // player
            { project_c::PREFAB_TYPE_BARBARIAN,     { "Barbarian.glb", "KayKit_Adventurers_1.0_FREE/Characters/gltf" }},

            // enemies
            { project_c::PREFAB_TYPE_SKELETON_WARRIOR, { "Skeleton_Warrior.glb", "KayKit_Skeletons_1.0_FREE/characters/gltf" }},

            // scene assets
            { project_c::PREFAB_TYPE_FLOOR,          { "Floor_Prototype.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            //{ project_c::PREFAB_FLOOR_OUTSIDE_REGION, { "Floor.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            
            //{ project_c::PREFAB_TYPE_WALL,      { "Wall.gltf",   "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            { project_c::PREFAB_TYPE_BOX,       { "Box_B.gltf",  "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            { project_c::PREFAB_TYPE_COIN_GOLD, { "Coin_A.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        };

        project_c::AppProjectC app_project_c(prefabs_data);
        app_project_c.run();

        return 0;
    }
    catch (const std::exception& e)
    {
        log(fmt::format("Exception: {}\n", e.what()));
        return -1;
    }
    catch (...)
    {
        log("Unknown exception\n");
        return -2;
    }
	return 0;
}