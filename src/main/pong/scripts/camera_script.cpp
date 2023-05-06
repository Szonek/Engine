#include "camera_script.h"

#include "global_constants.h"

pong::CameraScript::CameraScript(engine_application_t& app, engine_scene_t& scene)
    : IScript(app, scene)
{
    auto camera_comp = engineSceneAddCameraComponent(scene, go_);
    camera_comp.enabled = true;
    camera_comp.clip_plane_near = 0.1f;
    camera_comp.clip_plane_far = 100.0f;
    //camera_comp->type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
    //camera_comp->type_union.perspective_fov = 45.0f;
    camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
    camera_comp.type_union.orthographics_scale = K_CAMERA_ORTHO_SCALE;
    engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

    auto camera_transform_comp = engineSceneAddTransformComponent(scene, go_);
    camera_transform_comp.position[2] = 100.0f;
    engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);

}