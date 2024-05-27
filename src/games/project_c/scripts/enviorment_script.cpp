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
    auto scene = my_scene.get_handle();
    auto parent_go = project_c::utils::get_game_objects_with_name(scene, parent_name);
    if (parent_go.empty())
    {
        const auto env_parent = engineSceneCreateGameObject(scene);
        auto nc = engineSceneAddNameComponent(scene, env_parent);
        std::strncpy(nc.name, parent_name.data(), ENGINE_ENTITY_NAME_MAX_LENGTH);
        engineSceneUpdateNameComponent(scene, env_parent, &nc);
        parent_go.push_back(env_parent);
    }

    auto p = engineSceneAddParentComponent(scene, go);
    p.parent = parent_go.front();
    engineSceneUpdateParentComponent(scene, go, &p);
}
} // namespace 


project_c::Floor::Floor(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
    : EnviormentBaseScript(my_scene, go, "floor")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[0] += offset_x;
    tc.position[2] += offset_z;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.5f, 0.01f, 0.5f });
    engineSceneUpdateColliderComponent(scene, go_, &cc);
}

project_c::Wall::Wall(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
    : EnviormentBaseScript(my_scene, go, "wall")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[0] += offset_x;
    tc.position[2] += offset_z;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = false;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[1] = 0.5f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.5f, 0.5f, 0.5f});
    engineSceneUpdateColliderComponent(scene, go_, &cc);
}

project_c::Barrel::Barrel(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go, "barrel")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[1] -= 0.15f;
    tc.position[2] += 1.0f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[1] = 0.2f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.2f, 0.2f, 0.2f});
    }
    engineSceneUpdateColliderComponent(scene, go_, &cc);
}

project_c::MainLight::MainLight(engine::IScene* my_scene)
    : LightBaseScript(my_scene, "main-light")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    // position in world
    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 10.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.1f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.1f;

    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // for visulastuion add mesh component
    auto mc = engineSceneAddMeshComponent(scene, go_);
    mc.geometry = engineApplicationGetGeometryByName(my_scene_->get_app_handle(), "cube.glb");
    engineSceneUpdateMeshComponent(scene, go_, &mc);

    // and basic material
    auto mat = engineSceneAddMaterialComponent(scene, go_);
    mat.material = engineApplicationGetMaterialByName(my_scene_->get_app_handle(), "light_material");
    engineSceneUpdateMaterialComponent(scene, go_, &mat);

    // light component
    auto lc = engineSceneAddLightComponent(scene, go_);
    lc.type = ENGINE_LIGHT_TYPE_DIRECTIONAL;
    set_c_array(lc.intensity.ambient, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.intensity.specular, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.directional.direction, std::array<float, 3>{ 0.0f, 1.0f, 0.0f });
    engineSceneUpdateLightComponent(scene, go_, &lc);
}

project_c::PointLight::PointLight(engine::IScene* my_scene)
    : LightBaseScript(my_scene, "point-light")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    // position in world
    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = 3.0f;
    tc.position[1] = 1.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.1f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.1f;

    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // for visulastuion add mesh component
    auto mc = engineSceneAddMeshComponent(scene, go_);
    mc.geometry = engineApplicationGetGeometryByName(my_scene_->get_app_handle(), "cube.glb");
    engineSceneUpdateMeshComponent(scene, go_, &mc);

    // and basic material
    auto mat = engineSceneAddMaterialComponent(scene, go_);
    mat.material = engineApplicationGetMaterialByName(my_scene_->get_app_handle(), "light_material");
    engineSceneUpdateMaterialComponent(scene, go_, &mat);

    // light component
    auto lc = engineSceneAddLightComponent(scene, go_);
    lc.type = ENGINE_LIGHT_TYPE_POINT;
    set_c_array(lc.intensity.ambient, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
    set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    set_c_array(lc.intensity.specular, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    lc.point.constant = 1.0f;
    lc.point.linear = 0.09f;
    lc.point.quadratic = 0.032f;
    engineSceneUpdateLightComponent(scene, go_, &lc);
}

project_c::SpotLight::SpotLight(engine::IScene* my_scene)
    : LightBaseScript(my_scene, "spot-light")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    // position in world
    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = -3.0f;
    tc.position[1] = 1.0f;
    tc.position[2] = 0.0f;

    tc.scale[0] = 0.1f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.1f;

    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // for visulastuion add mesh component
    auto mc = engineSceneAddMeshComponent(scene, go_);
    mc.geometry = engineApplicationGetGeometryByName(my_scene_->get_app_handle(), "cube.glb");
    engineSceneUpdateMeshComponent(scene, go_, &mc);

    // and basic material
    auto mat = engineSceneAddMaterialComponent(scene, go_);
    mat.material = engineApplicationGetMaterialByName(my_scene_->get_app_handle(), "light_material");
    engineSceneUpdateMaterialComponent(scene, go_, &mat);

    // light component
    auto lc = engineSceneAddLightComponent(scene, go_);
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
    engineSceneUpdateLightComponent(scene, go_, &lc);
}

project_c::DebugPathNode::DebugPathNode(engine::IScene* my_scene, float offset_x, float offset_z)
    : BaseNode(my_scene, "debug-path-node")
{
    auto scene = my_scene_->get_handle();
    auto app = my_scene_->get_app_handle();

    add_parent_component_for_editor(*my_scene, go_, "debug_path");

    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = offset_x;
    tc.position[1] = 0.0f;
    tc.position[2] = offset_z;

    tc.scale[0] = 0.45f;
    tc.scale[1] = 0.1f;
    tc.scale[2] = 0.45f;

    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // for visulastuion add mesh component
    auto mc = engineSceneAddMeshComponent(scene, go_);
    mc.geometry = engineApplicationGetGeometryByName(app, "cube.glb");
    engineSceneUpdateMeshComponent(scene, go_, &mc);

    // and basic material
    auto mat = engineSceneAddMaterialComponent(scene, go_);
    mat.material = engineApplicationGetMaterialByName(app, "debug_path_node_mat");
    assert(ENGINE_INVALID_OBJECT_HANDLE != mat.material);
    engineSceneUpdateMaterialComponent(scene, go_, &mat);
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
