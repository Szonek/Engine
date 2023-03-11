#include <engine.h>

#include "application.h"
#include "scene.h"
#include "asset_store.h"

#include "logger.h"

namespace
{
inline engine::Scene* scene_cast(engine_scene_t engine_scene_t)
{
    return reinterpret_cast<engine::Scene*>(engine_scene_t);
}

inline entt::entity entity_cast(engine_game_object_t go)
{
    return static_cast<entt::entity>(go);
}

inline void transform_component_init(engine_tranform_component_t* comp)
{
    std::memset(comp, 0, sizeof(engine_tranform_component_t));
    comp->scale[0] = 1.0f;
    comp->scale[1] = 1.0f;
    comp->scale[2] = 1.0f;
}

inline void rect_transform_component_init(engine_rect_tranform_component_t* comp)
{
    std::memset(comp, 0, sizeof(engine_rect_tranform_component_t));
    comp->scale[0] = 1.0f;
    comp->scale[1] = 1.0f;
}

inline void camera_component_init(engine_camera_component_t* comp)
{
    std::memset(comp, 0, sizeof(engine_camera_component_t));
    // disabled by default?
    comp->enabled = 0;
    // clip
    comp->clip_plane_near = 0.1f;
    comp->clip_plane_far = 256.0f;
    // perspective
    comp->type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
    comp->type_union.perspective_fov = 45.0f;
    // default viewport
    comp->viewport_rect.x = 0.0f;
    comp->viewport_rect.y = 0.0f;
    comp->viewport_rect.width = 1.0f;
    comp->viewport_rect.height = 1.0f;
    // direction
    comp->direction.up[0] = 0.0f;
    comp->direction.up[1] = 1.0f;
    comp->direction.up[2] = 0.0f;
}

inline void material_component_init(engine_material_component_t* comp)
{
    std::memset(comp, 0, sizeof(engine_material_component_t));
    comp->shiness = 32;
}

inline void rigid_body_component_init(engine_rigid_body_component_t* comp)
{
    std::memset(comp, 0, sizeof(engine_rigid_body_component_t));
    comp->mass = 1.0f;
    comp->use_gravity = true;
}

inline void collider_component_init(engine_collider_component_t* comp)
{
    std::memset(comp, 0, sizeof(engine_collider_component_t));
    comp->type = ENGINE_COLLIDER_TYPE_BOX;

    comp->collider.box.center[0] = 0.0f;
    comp->collider.box.center[1] = 0.0f;
    comp->collider.box.center[2] = 0.0f;

    comp->collider.box.size[0] = 1.0f;
    comp->collider.box.size[1] = 1.0f;
    comp->collider.box.size[2] = 1.0f;
}

template<typename T>
inline void default_init(T*)
{
}


template<typename T, void(*F)(T*) = default_init>
inline T* add_component(engine_scene_t scene, engine_game_object_t engine_game_object_t)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(engine_game_object_t);
    auto ret = sc->add_component<T>(entity);
    F(ret);
    return ret;
}

template<typename T>
inline T* get_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(game_object);
    return sc->get_component<T>(entity);
}

template<typename T>
inline void remove_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(game_object);
    sc->remove_component<T>(entity);
}

template<typename T>
inline bool has_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(game_object);
    return sc->has_component<T>(entity);
}

} // namespace annonymous


void engineLog(const char* str)
{
    engine::log::log(engine::log::LogLevel::eTrace, str);
}

engine_result_code_t engineApplicationCreate(engine_application_t* handle, engine_application_create_desc_t create_desc)
{
    if (create_desc.asset_store_path)
    {
        //ToDo: make this per application. Multiple application would overwrite this singletons configurables.
        engine::AssetStore::get_instance().configure_base_path(create_desc.asset_store_path);
    }
	engine_result_code_t ret = ENGINE_RESULT_CODE_FAIL;
	*handle = reinterpret_cast<engine_application_t>(new engine::Application(create_desc, ret));

	return ret;
}

void engineApplicationDestroy(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	delete app;
}

bool engineApplicationIsKeyboardButtonDown(engine_application_t handle, engine_keyboard_keys_t key)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->keyboard_is_key_down(key);
}

bool engineApplicationIsKeyboardButtonUp(engine_application_t handle, engine_keyboard_keys_t key)
{
	return !engineApplicationIsKeyboardButtonDown(handle, key);
}

engine_mouse_coords_t engineApplicationGetMouseCoords(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->mouse_get_coords();
}

bool engineApplicationIsMouseButtonDown(engine_application_t handle, engine_mouse_button_t button)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->mouse_is_button_down(button);
}

bool engineApplicationIsMouseButtonUp(engine_application_t handle, engine_mouse_button_t button)
{
	return !engineApplicationIsMouseButtonDown(handle, button);
}

engine_application_frame_begine_info_t engineApplicationFrameBegine(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->begine_frame();
}

engine_result_code_t engineApplicationFrameRunScene(engine_application_t handle, engine_scene_t scene, float delta_time)
{
    if (!handle && !scene)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
	auto* app = reinterpret_cast<engine::Application*>(handle);
	auto* scene_typed = reinterpret_cast<engine::Scene*>(scene);
	return app->run_scene(scene_typed, delta_time);
}

engine_application_frame_end_info_t engineApplicationFrameEnd(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->end_frame();
}

ENGINE_API engine_result_code_t engineApplicationAddFontFromFile(engine_application_t handle, const char* name, engine_font_t* out)
{
    auto* app = reinterpret_cast<engine::Application*>(handle);
    *out = app->add_font_from_file(name);
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationAddGeometryFromMemory(engine_application_t handle, const engine_vertex_attribute_t* verts, size_t verts_count, uint32_t* inds, size_t inds_count, const char* name, engine_geometry_t* out)
{
    auto* app = reinterpret_cast<engine::Application*>(handle);
    *out = app->add_geometry_from_memory({ verts, verts_count}, {inds, inds_count}, name);
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationAddTexture2DFromMemory(engine_application_t handle, const engine_texture_2d_create_from_memory_desc_t& info, const char* name, engine_texture2d_t* out)
{
    auto* app = reinterpret_cast<engine::Application*>(handle);
    *out = app->add_texture_from_memory(info, name);
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationAddTexture2DFromFile(engine_application_t handle, const char* file_name, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out)
{
    auto* app = reinterpret_cast<engine::Application*>(handle);
    *out = app->add_texture_from_file(file_name, name, color_space);
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineSceneCreate(engine_scene_t* out)
{
    engine_result_code_t ret;
    *out = reinterpret_cast<engine_scene_t>(new engine::Scene(ret));
    assert(ENGINE_INVALID_GAME_OBJECT_ID == engineSceneCreateGameObject(*out)); // add invalid game object id
    return ret;
}

void engineSceneDestroy(engine_scene_t scene)
{
    if (scene)
    {
        auto sc = scene_cast(scene);
        delete sc;
    }
}


engine_game_object_t engineSceneCreateGameObject(engine_scene_t scene)
{
    auto sc = scene_cast(scene);
    auto new_entity = sc->create_new_entity();
    return static_cast<uint32_t>(new_entity);
}

void engineSceneDestroyGameObject(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = scene_cast(scene);
    sc->destroy_entity(entity_cast(game_object));
}

void engineSceneSetGravityVector(engine_scene_t scene, const float gravity[3])
{
    auto sc = scene_cast(scene);
    sc->set_physcis_gravity(std::array<float, 3>{gravity[0], gravity[1], gravity[2]});
}

engine_name_component_t* engineSceneAddNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_name_component_t>(scene, game_object);
}

engine_name_component_t* engineSceneGetNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_name_component_t>(scene, game_object);
}

void engineSceneRemoveNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_name_component_t>(scene, game_object);
}

bool engineSceneHasNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_name_component_t>(scene, game_object);
}


engine_tranform_component_t* engineSceneAddTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_tranform_component_t, transform_component_init>(scene, game_object);
}

engine_tranform_component_t* engineSceneGetTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_tranform_component_t>(scene, game_object);
}

void engineSceneRemoveTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_tranform_component_t>(scene, game_object);
}

bool engineSceneHasTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_tranform_component_t>(scene, game_object);
}

engine_rect_tranform_component_t* engineSceneAddRectTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_rect_tranform_component_t, rect_transform_component_init>(scene, game_object);
}

engine_rect_tranform_component_t* engineSceneGetRectTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_rect_tranform_component_t>(scene, game_object);
}

void engineSceneRemoveRectTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_rect_tranform_component_t>(scene, game_object);
}

bool engineSceneHasRectTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_rect_tranform_component_t>(scene, game_object);
}

engine_mesh_component_t* engineSceneAddMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_mesh_component_t>(scene, game_object);
}

engine_mesh_component_t* engineSceneGetMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_mesh_component_t>(scene, game_object);
}

void engineSceneRemoveMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_mesh_component_t>(scene, game_object);
}

bool engineSceneHasMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_mesh_component_t>(scene, game_object);
}

engine_material_component_t* engineSceneAddMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_material_component_t, material_component_init>(scene, game_object);
}

engine_material_component_t* engineSceneGetMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_material_component_t>(scene, game_object);
}

void engineSceneRemoveMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_material_component_t>(scene, game_object);
}

bool engineSceneHasMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_material_component_t>(scene, game_object);
}

engine_light_component_t* engineSceneAddLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_light_component_t>(scene, game_object);
}

engine_light_component_t* engineSceneGetLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_light_component_t>(scene, game_object);
}

void engineSceneRemoveLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_light_component_t>(scene, game_object);
}

bool engineSceneHasLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_light_component_t>(scene, game_object);
}

engine_camera_component_t* engineSceneAddCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_camera_component_t, camera_component_init>(scene, game_object);
}

engine_camera_component_t* engineSceneGetCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_camera_component_t>(scene, game_object);
}

void engineSceneRemoveCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_camera_component_t>(scene, game_object);
}

bool engineSceneHasCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_camera_component_t>(scene, game_object);
}

engine_text_component_t* engineSceneAddTextComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_text_component_t>(scene, game_object);
}

engine_text_component_t* engineSceneGetTextComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_text_component_t>(scene, game_object);
}

void engineSceneRemoveTextComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_text_component_t>(scene, game_object);
}

bool engineSceneHasTextComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_text_component_t>(scene, game_object);
}

engine_rigid_body_component_t* engineSceneAddRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_rigid_body_component_t, rigid_body_component_init>(scene, game_object);
}

engine_rigid_body_component_t* engineSceneGetRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_rigid_body_component_t>(scene, game_object);
}

void engineSceneRemoveRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_rigid_body_component_t>(scene, game_object);
}

bool engineSceneHasRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_rigid_body_component_t>(scene, game_object);
}

engine_collider_component_t* engineSceneAddColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_collider_component_t, collider_component_init>(scene, game_object);
}

engine_collider_component_t* engineSceneGetColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_collider_component_t>(scene, game_object);
}

void engineSceneRemoveColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_collider_component_t>(scene, game_object);
}

bool engineSceneHasColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_collider_component_t>(scene, game_object);
}
