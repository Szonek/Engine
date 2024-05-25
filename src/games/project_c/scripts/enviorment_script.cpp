#include "enviorment_script.h"

#include "iscene.h"

project_c::Floor::Floor(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
    : BaseNode(my_scene, go, "floor")
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
    : BaseNode(my_scene, go, "wall")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[0] += offset_x;
    tc.position[2] += offset_z;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    //auto cc = engineSceneAddColliderComponent(scene, go_);
    //cc.type = ENGINE_COLLIDER_TYPE_BOX;
    //set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.5f, 0.01f, 0.5f });
    //engineSceneUpdateColliderComponent(scene, go_, &cc);
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
    : BaseNode(my_scene, "main-light")
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
    : BaseNode(my_scene, "point-light")
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
    : BaseNode(my_scene, "spot-light")
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