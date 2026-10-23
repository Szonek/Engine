#include "enviorment_script.h"
#include "iscene.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace
{
// lists the game object in the given folder of the editor's scene hierarchy
void set_editor_folder(engine_game_object_t go, std::string_view path)
{
    auto fc = engineGameObjectAddFolderComponent(go);
    std::strncpy(fc.path, path.data(), std::min<std::size_t>(path.size(), ENGINE_FOLDER_PATH_MAX_LENGTH - 1));
    engineGameObjectUpdateFolderComponent(go, &fc);
}
} // namespace

project_c::Floor::Floor(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

project_c::DebugPathNode::DebugPathNode(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
    : BaseNode(my_scene, go, "debug-path-node")
{
    set_editor_folder(go_, "debug_path");

    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = offset_x;
    tc.position[1] = 0.0f;
    tc.position[2] = offset_z;

    tc.scale[0] = 0.45f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.45f;

    engineGameObjectUpdateTransformComponent(go_, &tc);

    // for visulastuion add mesh component
    const auto cube_geo = engineGeometryGetByName("cube.glb");
    if (ENGINE_INVALID_OBJECT_HANDLE != cube_geo)
    {
        auto mc = engineGameObjectAddMeshComponent(go_);
        mc.geometry = cube_geo;
        engineGameObjectUpdateMeshComponent(go_, &mc);
    }

    // and basic material
    auto mat = engineGameObjectAddMaterialComponent(go_);
    set_c_array(mat.diffuse_color, std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f});
    engineGameObjectUpdateMaterialComponent(go_, &mat);
}
