#include <engine.h>

#include "application.h"
#include "application_editor.h"
#include "scene.h"
#include "asset_store.h"
#include "ui_document.h"

#include "logger.h"

#include <utility>

#include <fmt/format.h>

namespace
{
inline engine::Application* application_cast(engine_application_t engine_app)
{
    return reinterpret_cast<engine::Application*>(engine_app);
}

inline engine::UiDocument* ui_document_cast(engine_ui_document_t doc)
{
    return reinterpret_cast<engine::UiDocument*>(doc);
}

inline engine::UiDataHandle* ui_data_handle_cast(engine_ui_data_handle_t handle)
{
    return reinterpret_cast<engine::UiDataHandle*>(handle);
}

inline engine::Scene* scene_cast(engine_scene_t engine_scene_t)
{
    return reinterpret_cast<engine::Scene*>(engine_scene_t);
}

inline entt::entity entity_cast(engine_game_object_t go)
{
    return static_cast<entt::entity>(go);
}

inline entt::runtime_view* runtime_view_cast(engine_component_view_t comp_view)
{
    return reinterpret_cast<entt::runtime_view*>(comp_view);
}

inline auto component_iterator_cast(engine_component_iterator_t it)
{
    return reinterpret_cast<decltype(std::declval<entt::runtime_view>().begin())*>(it);
}

template<typename T>
inline T add_component(engine_scene_t scene, engine_game_object_t engine_game_object_t)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(engine_game_object_t);
    auto ret = sc->add_component<T>(entity);
    return *ret;
}

template<typename T>
inline T get_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(game_object);
    return *sc->get_component<T>(entity);
}

template<typename T>
inline void update_component(engine_scene_t scene, engine_game_object_t game_object, const T* comp)
{
    auto sc = scene_cast(scene);
    auto entity = entity_cast(game_object);
    sc->update_component<T>(entity, *comp);
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

    if (create_desc.enable_editor)
    {
        *handle = reinterpret_cast<engine_application_t>(new engine::ApplicationEditor(create_desc, ret));
    }
    else
    {
        *handle = reinterpret_cast<engine_application_t>(new engine::Application(create_desc, ret));
    }

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

engine_coords_2d_t engineApplicationGetMouseCoords(engine_application_t handle)
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

bool engineApplicationGetFingerInfo(engine_application_t handle, engine_fingers_infos_list_t* infos_list)
{
    if (!infos_list)
    {
        return false;
    }
    auto* app = reinterpret_cast<engine::Application*>(handle);
    const auto finger_list = app->get_finger_info_events();
    if(finger_list.empty())
    {
        std::memset(infos_list->infos, 0, sizeof(engine_fingers_infos_list_t));
        return false;
    }
    std::memcpy(infos_list->infos, finger_list.data(), sizeof(engine_fingers_infos_list_t));
    return true;
}

engine_application_frame_begine_info_t engineApplicationFrameBegine(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->begine_frame();
}

engine_result_code_t engineApplicationFrameSceneUpdatePhysics(engine_application_t handle, engine_scene_t scene, float delta_time)
{
    if (!handle && !scene)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    //auto* app = application_cast(handle);
    auto* scene_typed = scene_cast(scene);
    return scene_typed->physics_update(delta_time);
}

engine_result_code_t engineApplicationFrameSceneUpdateGraphics(engine_application_t handle, engine_scene_t scene, float delta_time)
{
    if (!handle && !scene)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
	auto* app = reinterpret_cast<engine::Application*>(handle);
	auto* scene_typed = reinterpret_cast<engine::Scene*>(scene);
	return app->update_scene(scene_typed, delta_time);
}

engine_application_frame_end_info_t engineApplicationFrameEnd(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->end_frame();
}

engine_result_code_t engineApplicationAddFontFromFile(engine_application_t handle, const char* file_name, const char* handle_name)
{
    auto* app = reinterpret_cast<engine::Application*>(handle);
    const auto result = app->add_font_from_file(file_name, handle_name);
    return result ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engineApplicationAddGeometryFromDesc(engine_application_t handle, const engine_geometry_create_desc_t* desc, const char* name, engine_geometry_t* out)
{
    auto* app = reinterpret_cast<engine::Application*>(handle);
    const auto ret = app->add_geometry(desc->verts_layout, desc->verts_count, { reinterpret_cast<const std::byte*>(desc->verts_data), desc->verts_data_size }, { desc->inds, desc->inds_count}, name);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created geometry: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_geometry_t engineApplicationGetGeometryByName(engine_application_t handle, const char* name)
{
    const auto* app = application_cast(handle);
    return app->get_geometry(name);
}

engine_geometry_attribute_limit_t engineApplicationGeometryGetAttributeLimits(engine_application_t handle, engine_geometry_t geometry, engine_vertex_attribute_type_t type)
{
    engine_geometry_attribute_limit_t ret{};
    ret.elements_count = 0;
    if (!handle || geometry == ENGINE_INVALID_OBJECT_HANDLE || type == ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT)
    {
        return ret;
    }
    const auto* app = application_cast(handle);
    const auto geometry_obj = app->get_geometry(geometry);
    const auto attrib = geometry_obj->get_vertex_attribute(type);
    ret.elements_count = attrib.range_max.size();
    for (auto i = 0; i < ret.elements_count; i++)
    {
        ret.max[i] = attrib.range_max[i];
        ret.min[i] = attrib.range_min[i];
    }
    return ret;
}

engine_result_code_t engineApplicationAddMaterialFromDesc(engine_application_t handle, const engine_material_create_desc_t* desc, const char* name, engine_material_t* out)
{
    if (!handle || !desc || !name)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = reinterpret_cast<engine::Application*>(handle);
    const auto ret = app->add_material(*desc, name);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created material: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_material_t engineApplicationGetMaterialByName(engine_application_t handle, const char* name)
{
    const auto* app = application_cast(handle);
    return app->get_material(name);
}

engine_result_code_t engineApplicationAddTexture2DFromDesc(engine_application_t handle, const engine_texture_2d_create_desc_t* info, const char* name, engine_texture2d_t* out)
{
    auto* app = application_cast(handle);
    const auto ret =  app->add_texture(*info, name);

    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created texture: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationAddTexture2DFromFile(engine_application_t handle, const char* file_name, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out)
{
    auto* app = application_cast(handle);
    const auto ret = app->add_texture_from_file(file_name, name, color_space);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created texture from file: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_texture2d_t engineApplicationGetTextured2DByName(engine_application_t handle, const char* name)
{
    const auto* app = application_cast(handle);
    return app->get_texture(name);
}

engine_result_code_t engineApplicationAllocateModelDescAndLoadDataFromFile(engine_application_t handle, engine_model_specification_t spec, const char *file_name, const char* base_dir, engine_model_desc_t* out)
{
    if (!out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = application_cast(handle);
    *out = app->load_model_desc_from_file(spec, file_name, base_dir);
    if (!out->internal_handle)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return ENGINE_RESULT_CODE_OK;
}

void engineApplicationReleaseModelDesc(engine_application_t handle, engine_model_desc_t* model_info)
{
    auto* app = application_cast(handle);
    app->release_model_desc(model_info);
}

engine_result_code_t engineApplicationSceneCreate(engine_application_t handle, engine_scene_create_desc_t desc, engine_scene_t* out)
{
    if (!handle)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = application_cast(handle);
    *out = reinterpret_cast<engine_scene_t>(app->create_scene(desc));
    if (!out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    assert(ENGINE_INVALID_OBJECT_HANDLE != engineSceneCreateGameObject(*out)); // add invalid game object id
    return ENGINE_RESULT_CODE_OK;
}

void engineApplicationSceneDestroy(engine_application_t handle, engine_scene_t scene)
{
    if (!handle || !scene)
    {
        return;
    }
    if (scene)
    {
        auto* app = application_cast(handle);
        app->release_scene(scene_cast(scene));
    }
}


engine_game_object_t engineSceneCreateGameObject(engine_scene_t scene)
{
    auto sc = scene_cast(scene);
    auto new_entity = sc->create_new_entity();
    return static_cast<engine_game_object_t>(new_entity);
}

void engineSceneDestroyGameObject(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = scene_cast(scene);
    sc->destroy_entity(entity_cast(game_object));
}

void engineScenePhysicsSetGravityVector(engine_scene_t scene, const float gravity[3])
{
    auto sc = scene_cast(scene);
    sc->set_physcis_gravity(std::array<float, 3>{gravity[0], gravity[1], gravity[2]});
}

void engineScenePhysicsGetCollisions(engine_scene_t scene, size_t* num_collision, const engine_collision_info_t** collisions)
{
    auto sc = scene_cast(scene);
    sc->get_physcis_collisions_list(*collisions, num_collision);
}

engine_ray_hit_info_t engineScenePhysicsRayCast(engine_scene_t scene, const engine_ray_t* ray, float max_distance)
{
    auto sc = scene_cast(scene);
    return sc->raycast_into_physics_world(*ray, max_distance);
}

engine_result_code_t engineApplicationCreateUiDocumentDataHandle(engine_application_t app, const char* name, const engine_ui_document_data_binding_t* bindings, size_t bindings_count, engine_ui_data_handle_t* out)
{
    if (bindings_count == 0 && !bindings)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }

    if (app && name && out)
    {
        auto* app_handle = application_cast(app);
        auto ret = new engine::UiDataHandle(app_handle->create_ui_document_data_handle(name, { bindings, bindings_count}));
        if (ret)
        {
            *out = reinterpret_cast<engine_ui_data_handle_t>(ret);
            return ENGINE_RESULT_CODE_OK;
        }
    }
    return ENGINE_RESULT_CODE_FAIL;
}

void engineUiDataHandleDestroy(engine_ui_data_handle_t handle)
{
    if (handle)
    {
        auto* data_handle = ui_data_handle_cast(handle);
        delete data_handle;
    }
}

void engineUiDataHandleDirtyAllVariables(engine_ui_data_handle_t handle)
{
    if (handle)
    {
        auto* data_handle = ui_data_handle_cast(handle);
        data_handle->dirty_all_variables();
    }
}

void engineUiDataHandleDirtyVariable(engine_ui_data_handle_t handle, const char* name)
{
    if (handle)
    {
        auto* data_handle = ui_data_handle_cast(handle);
        data_handle->dirty_variable(name);
    }
}

engine_result_code_t engineApplicationCreateUiDocumentFromFile(engine_application_t app, const char* file_path, engine_ui_document_t* out)
{
    if (app && file_path && out)
    {
        auto* app_handle = application_cast(app);
        auto* ret = new engine::UiDocument(app_handle->load_ui_document(file_path));
        if (ret)
        {
            *out = reinterpret_cast<engine_ui_document_t>(ret);
            return ENGINE_RESULT_CODE_OK;
        }    
    }
    return ENGINE_RESULT_CODE_FAIL;
}

void engineApplicationUiDocumentDestroy(engine_ui_document_t doc)
{
    if (doc)
    {
        auto* doc_handle = ui_document_cast(doc);
        delete doc_handle;  
    }
}

void engineUiDocumentShow(engine_ui_document_t ui_doc)
{
    if (ui_doc)
    {
        auto* doc = ui_document_cast(ui_doc);
        doc->show();
    }
}

void engineUiDocumentHide(engine_ui_document_t ui_doc)
{
    if (ui_doc)
    {
        auto* doc = ui_document_cast(ui_doc);
        doc->hide();
    }
}

engine_result_code_t engineUiDocumentGetElementById(engine_ui_document_t document, const char* id, engine_ui_element_t* out)
{
    static std::map<std::string, engine::UiElement> ui_elements_cache;
    engine_result_code_t ret = ENGINE_RESULT_CODE_FAIL;
    if (document && id && out)
    {
        auto doc = ui_document_cast(document);
        auto element = doc->get_element_by_id(id, ret);
        if (ret == ENGINE_RESULT_CODE_OK)
        {
            *out = reinterpret_cast<engine_ui_element_t>(element);
        }
    }
    return ret;
}

engine_result_code_t engineUiElementAddEventCallback(engine_ui_element_t element, engine_ui_event_type_t event_type, void* user_data, void(*callback)(const engine_ui_event_t*, void*))
{
    if (element && event_type && callback)
    {
        auto element_handle = reinterpret_cast<engine::UiElement*>(element);
        element_handle->register_callback(event_type, user_data, callback);
        return ENGINE_RESULT_CODE_OK;
    }

    return ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engineUiElementSetProperty(engine_ui_element_t element, const char* property, const char* value)
{
    bool result = false;
    if (element && property && value)
    {
        auto element_handle = reinterpret_cast<engine::UiElement*>(element);
        result = element_handle->set_property(property, value);
    }
    return result ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

void engineUiElementRemoveProperty(engine_ui_element_t element, const char* property)
{
    if (element && property)
    {
        auto element_handle = reinterpret_cast<engine::UiElement*>(element);
        element_handle->remove_property(property);
    }
}

engine_result_code_t engineCreateComponentView(engine_component_view_t* out)
{
    if (out)
    {
        *out = reinterpret_cast<engine_component_view_t>(new entt::runtime_view());
        return ENGINE_RESULT_CODE_OK;
    }

    return ENGINE_RESULT_CODE_FAIL;
}

void engineDestroyComponentView(engine_component_view_t view)
{
    if (view)
    {
        auto rv = runtime_view_cast(view);
        delete rv;
    }
}

engine_result_code_t engineComponentViewCreateBeginComponentIterator(engine_component_view_t view, engine_component_iterator_t* out)
{
    if (view && out)
    {
        auto rv = runtime_view_cast(view);
        *out = reinterpret_cast<engine_component_iterator_t>(new decltype(rv->begin())(rv->begin()));
        return ENGINE_RESULT_CODE_OK;
    }
    return ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engineComponentViewCreateEndComponentIterator(engine_component_view_t view, engine_component_iterator_t* out)
{
    if (view && out)
    {
        auto rv = runtime_view_cast(view);
        *out = reinterpret_cast<engine_component_iterator_t>(new decltype(rv->end())(rv->end()));
        return ENGINE_RESULT_CODE_OK;
    }
    return ENGINE_RESULT_CODE_FAIL;
}

void engineComponentIteratorNext(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it_typed = component_iterator_cast(iterator);
        (*it_typed)++;
    }
}

engine_game_object_t engineComponentIteratorGetGameObject(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it_typed = component_iterator_cast(iterator);
        return static_cast<engine_game_object_t>(**it_typed);
    }
    return ENGINE_INVALID_GAME_OBJECT_ID;
}

bool engineComponentIteratorCheckEqual(engine_component_iterator_t lhs, engine_component_iterator_t rhs)
{
    bool ret = false;
    if (lhs && rhs)
    {
        auto lhs_typed = component_iterator_cast(lhs);
        auto rhs_typed = component_iterator_cast(rhs);
        ret = (*lhs_typed == *rhs_typed);
    }
    return ret;
}

void engineDeleteComponentIterator(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it = component_iterator_cast(iterator);
        delete it;
    }
}

engine_name_component_t engineSceneAddNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_name_component_t>(scene, game_object);
}

engine_name_component_t engineSceneGetNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_name_component_t>(scene, game_object);
}

void engineSceneUpdateNameComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_name_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_name_component_t>(scene, game_object);
}

bool engineSceneHasNameComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_name_component_t>(scene, game_object);
}
void engineSceneComponentViewAttachNameComponent(engine_scene_t scene, engine_component_view_t view)
{
    auto sc = scene_cast(scene);
    auto rv = runtime_view_cast(view);
    sc->attach_component_to_runtime_view<engine_name_component_t>(*rv);
}

engine_tranform_component_t engineSceneAddTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_tranform_component_t>(scene, game_object);
}

engine_tranform_component_t engineSceneGetTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_tranform_component_t>(scene, game_object);
}

void engineSceneUpdateTransformComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_tranform_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_tranform_component_t>(scene, game_object);
}

bool engineSceneHasTransformComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_tranform_component_t>(scene, game_object);
}

engine_mesh_component_t engineSceneAddMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_mesh_component_t>(scene, game_object);
}

engine_mesh_component_t engineSceneGetMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_mesh_component_t>(scene, game_object);
}

void engineSceneUpdateMeshComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_mesh_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_mesh_component_t>(scene, game_object);
}

bool engineSceneHasMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_mesh_component_t>(scene, game_object);
}

// skinned mesh
engine_skin_component_t engineSceneAddSkinComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_skin_component_t>(scene, game_object);
}

engine_skin_component_t engineSceneGetSkinComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_skin_component_t>(scene, game_object);
}

void engineSceneUpdateSkinComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_skin_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveSkinComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_skin_component_t>(scene, game_object);
}

bool engineSceneHasSkinComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_skin_component_t>(scene, game_object);
}
// -- 

// bone
engine_bone_component_t engineSceneAddBoneComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_bone_component_t>(scene, game_object);
}

engine_bone_component_t engineSceneGetBoneComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_bone_component_t>(scene, game_object);
}

void engineSceneUpdateBoneComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_bone_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveBoneComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_bone_component_t>(scene, game_object);
}

bool engineSceneHasBoneComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_bone_component_t>(scene, game_object);
}
// -- 

engine_material_component_t engineSceneAddMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_material_component_t>(scene, game_object);
}

engine_material_component_t engineSceneGetMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_material_component_t>(scene, game_object);
}

void engineSceneUpdateMaterialComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_material_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_material_component_t>(scene, game_object);
}

bool engineSceneHasMaterialComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_material_component_t>(scene, game_object);
}

engine_light_component_t engineSceneAddLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_light_component_t>(scene, game_object);
}

engine_light_component_t engineSceneGetLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_light_component_t>(scene, game_object);
}

void engineSceneUpdateLightComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_light_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_light_component_t>(scene, game_object);
}

bool engineSceneHasLightComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_light_component_t>(scene, game_object);
}

engine_camera_component_t engineSceneAddCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_camera_component_t>(scene, game_object);
}

engine_camera_component_t engineSceneGetCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_camera_component_t>(scene, game_object);
}

void engineSceneUpdateCameraComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_camera_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_camera_component_t>(scene, game_object);
}

bool engineSceneHasCameraComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_camera_component_t>(scene, game_object);
}

void engineSceneComponentViewAttachCameraComponent(engine_scene_t scene, engine_component_view_t view)
{
    auto sc = scene_cast(scene);
    auto rv = runtime_view_cast(view);
    sc->attach_component_to_runtime_view<engine_camera_component_t>(*rv);
}

engine_rigid_body_component_t engineSceneAddRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_rigid_body_component_t>(scene, game_object);
}

engine_rigid_body_component_t engineSceneGetRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_rigid_body_component_t>(scene, game_object);
}

void engineSceneUpdateRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_rigid_body_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_rigid_body_component_t>(scene, game_object);
}

bool engineSceneHasRigidBodyComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_rigid_body_component_t>(scene, game_object);
}

engine_collider_component_t engineSceneAddColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_collider_component_t>(scene, game_object);
}

engine_collider_component_t engineSceneGetColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_collider_component_t>(scene, game_object);
}

void engineSceneUpdateColliderComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_collider_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_collider_component_t>(scene, game_object);
}

bool engineSceneHasColliderComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_collider_component_t>(scene, game_object);
}

engine_parent_component_t engineSceneAddParentComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_parent_component_t>(scene, game_object);
}

engine_parent_component_t engineSceneGetParentComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_parent_component_t>(scene, game_object);
}

void engineSceneUpdateParentComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_parent_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveParentComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_parent_component_t>(scene, game_object);
}

bool engineSceneHasParentComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_parent_component_t>(scene, game_object);
}