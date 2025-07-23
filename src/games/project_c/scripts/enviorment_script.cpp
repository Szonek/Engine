#include "enviorment_script.h"
#include "scripts_utils.h"
#include "iscene.h"
#include "iapplication.h"

namespace
{
void add_parent_component_for_editor(engine::IScene& my_scene, engine_game_object_t go , std::string_view parent_name)
{
    if (!my_scene.get_app()->is_editor_enabled())
    {
        return;
    }
    auto parent_go = project_c::utils::get_game_objects_with_name(parent_name);
    if (parent_go.empty())
    {
        const auto env_parent = engineGameObjectCreate();
        auto nc = engineGameObjectAddNameComponent(env_parent);
        std::strncpy(nc.name, parent_name.data(), ENGINE_ENTITY_NAME_MAX_LENGTH);
        engineGameObjectUpdateNameComponent(env_parent, &nc);
        parent_go.push_back(env_parent);
    }

    auto p = engineGameObjectAddParentComponent(go);
    p.parent = parent_go.front();
    engineGameObjectUpdateParentComponent(go, &p);
}
} // namespace 

project_c::Floor::Floor(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
    : EnviormentBaseScript(my_scene, go, "floor")
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] += offset_x;
    tc.position[2] += offset_z;
    tc.scale[0] = 0.25f;
    tc.scale[1] = 0.25f;
    tc.scale[2] = 0.25f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc.collider.box.size, std::array<float, 3>{ 2.0f, 0.5f, 2.0f });
    engineGameObjectUpdateColliderComponent(go_, &cc);
}

project_c::Wall::Wall(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z, float y_rotation)
    : EnviormentBaseScript(my_scene, go, "wall")
{

    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] += offset_x;
    tc.position[2] += offset_z;
    tc.scale[0] = 0.25f;
    tc.scale[1] = 0.25f;
    tc.scale[2] = 0.25f;
    // rotate by 90 degrees
    const glm::quat rot = glm::angleAxis(glm::radians(y_rotation), glm::vec3(0.0f, 1.0f, 0.0f));
    tc.rotation[0] = rot.x;
    tc.rotation[1] = rot.y;
    tc.rotation[2] = rot.z;
    tc.rotation[3] = rot.w;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // physcis
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = false;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[1] = 2.0f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 2.0f, 2.0f, 0.5f});
    engineGameObjectUpdateColliderComponent(go_, &cc);
}


project_c::FloorOutsideRegion::FloorOutsideRegion(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "primitive-cube")
{
    add_parent_component_for_editor(*my_scene, go_, "enviorment");

    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.scale[0] = 0.25f;
    tc.scale[1] = 0.25f;
    tc.scale[2] = 0.25f;
    engineGameObjectUpdateTransformComponent(go_, &tc);
}

project_c::Barrel::Barrel(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "barrel")
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[1] -= 0.15f;
    tc.position[2] += 1.0f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // physcis
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[1] = 0.2f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.2f, 0.2f, 0.2f});
    }
    engineGameObjectUpdateColliderComponent(go_, &cc);
}

project_c::MainLight::MainLight(engine::IScene* my_scene)
    : LightBaseScript(my_scene, "main-light")
{
    // position in world
    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 10.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.1f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.1f;

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
    engineGameObjectUpdateMaterialComponent(go_, &mat);

    // light component
    auto lc = engineGameObjectAddLightComponent(go_);
    lc.type = ENGINE_LIGHT_TYPE_DIRECTIONAL;
    set_c_array(lc.intensity.ambient, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.intensity.specular, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.directional.direction, std::array<float, 3>{ 0.0f, 1.0f, 0.0f });
    engineGameObjectUpdateLightComponent(go_, &lc);
}

project_c::PointLight::PointLight(engine::IScene* my_scene)
    : LightBaseScript(my_scene, "point-light")
{
    // position in world
    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = 3.0f;
    tc.position[1] = 1.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.1f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.1f;

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
    engineGameObjectUpdateMaterialComponent(go_, &mat);

    // light component
    auto lc = engineGameObjectAddLightComponent(go_);
    lc.type = ENGINE_LIGHT_TYPE_POINT;
    set_c_array(lc.intensity.ambient, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    set_c_array(lc.intensity.specular, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    lc.point.constant = 1.0f;
    lc.point.linear = 0.09f;
    lc.point.quadratic = 0.032f;
    engineGameObjectUpdateLightComponent(go_, &lc);
}

project_c::SpotLight::SpotLight(engine::IScene* my_scene)
    : LightBaseScript(my_scene, "spot-light")
{
    // position in world
    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = -3.0f;
    tc.position[1] = 1.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.1f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.1f;

    engineGameObjectUpdateTransformComponent(go_, &tc);

    // for visulastuion add mesh component
    const auto cube_geo = engineGeometryGetByName( "cube.glb");
    if (ENGINE_INVALID_OBJECT_HANDLE != cube_geo)
    {
        auto mc = engineGameObjectAddMeshComponent(go_);
        mc.geometry = cube_geo;
        engineGameObjectUpdateMeshComponent(go_, &mc);
    }

    // and basic material
    auto mat = engineGameObjectAddMaterialComponent(go_);
    engineGameObjectUpdateMaterialComponent(go_, &mat);

    // light component
    auto lc = engineGameObjectAddLightComponent(go_);
    lc.type = ENGINE_LIGHT_TYPE_SPOT;
    set_c_array(lc.intensity.ambient, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    set_c_array(lc.intensity.specular, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    set_c_array(lc.spot.direction, std::array<float, 3>{ 0.0f, -1.0f, 0.0f });
    lc.spot.cut_off = 12.5f;
    lc.spot.outer_cut_off = 17.5f;
    lc.spot.constant = 1.0f;
    lc.spot.linear = 0.09f;
    lc.spot.quadratic = 0.032f;
    engineGameObjectUpdateLightComponent(go_, &lc);
}

project_c::DebugPathNode::DebugPathNode(engine::IScene* my_scene, float offset_x, float offset_z)
    : BaseNode(my_scene, "debug-path-node")
{
    add_parent_component_for_editor(*my_scene, go_, "debug_path");

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
    set_c_array(mat.data.pong.diffuse_color, std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f});
    engineGameObjectUpdateMaterialComponent(go_, &mat);
}

project_c::EnviormentBaseScript::EnviormentBaseScript(engine::IScene* my_scene, engine_game_object_t go, std::string_view name)
    : BaseNode(my_scene, go, name)
{
    add_parent_component_for_editor(*my_scene, go_, "enviorment");
}

project_c::LightBaseScript::LightBaseScript(engine::IScene* my_scene, std::string_view name)
    : BaseNode(my_scene, name)
{
    add_parent_component_for_editor(*my_scene, go_, "lights");
}

