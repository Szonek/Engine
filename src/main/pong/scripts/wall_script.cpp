#include "wall_script.h"
#include "global_constants.h"

#include "utils.h"

#include <cassert>

WallScript::WallScript(engine_application_t& app, engine_scene_t& scene, float init_pos_y, const char* name)
    : IScript(app, scene)
{
    auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
    mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "cube");
    assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player goal net script!");
    engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = 0.0f;
    tc.position[1] = init_pos_y;
    tc.position[2] = 0.0f;

    tc.scale[0] = 12.0f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 1.0f;
    engineSceneUpdateTransformComponent(scene_, go_, &tc);

    auto bc = engineSceneAddColliderComponent(scene, go_);
    bc.type = ENGINE_COLLIDER_TYPE_BOX;
    bc.bounciness = 1.0f;
    bc.friction_static = 0.0f;
    engineSceneUpdateColliderComponent(scene, go_, &bc);

    auto material_comp = engineSceneAddMaterialComponent(scene, go_);
    set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.2f });
    engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    auto nc = engineSceneAddNameComponent(scene, go_);
    std::strcpy(nc.name, name);
    engineSceneUpdateNameComponent(scene, go_, &nc);
}

WallTopScript::WallTopScript(engine_application_t& app, engine_scene_t& scene)
    : WallScript(app, scene, K_WALL_Y_OFFSET, "top_wall")
{

}

BottomTopScript::BottomTopScript(engine_application_t& app, engine_scene_t& scene)
: WallScript(app, scene, -1.0f * K_WALL_Y_OFFSET, "bottom_wall")
{

}