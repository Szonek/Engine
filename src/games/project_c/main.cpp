#include <engine.h>

#include "app.h"

int main(int argc, char** argv)
{
    try
    {
        const std::unordered_map<project_c::PrefabType, std::pair<std::string, std::string>> prefabs_data =
        {
            { project_c::PREFAB_TYPE_BARBARIAN,     { "KayKit_Adventurers_1.0_FREE/Characters/gltf/Barbarian.glb", "KayKit_Adventurers_1.0_FREE/Characters/gltf" }},
            //{ project_c::PREFAB_TYPE_DAGGER,       { "dagger_01.glb", ""}},
            //{ project_c::PREFAB_TYPE_SWORD,        { "weapon-sword.glb", "Textures_mini_arena" }},
            //{ project_c::PREFAB_TYPE_SOLIDER,      { "character-soldier.glb", "Textures_mini_arena" }},
            //{ project_c::PREFAB_TYPE_ORC,          { "character-orc.glb", "Textures_mini_dungeon" }},
            //{ project_c::PREFAB_TYPE_BARREL,       { "barrel.glb", "Textures_mini_dungeon" }},
            { project_c::PREFAB_TYPE_FLOOR,        { "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Floor.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            { project_c::PREFAB_TYPE_FLOOR_DETAIL, { "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Floor_Dirt.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            { project_c::PREFAB_TYPE_WALL,         { "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Wall.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
            //{ project_c::PREFAB_TYPE_CUBE,         { "cube.glb", ""}},
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