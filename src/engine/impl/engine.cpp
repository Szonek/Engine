#include <engine.h>

#include "application.h"
#include "application_editor.h"
#include "scene.h"
#include "asset_store.h"
#include "ui_document.h"
#include "collision_desc.h"
#include "gltf_parser.h"
#include "skin.h"

#include "logger.h"

#include <utility>

#include <fmt/format.h>

namespace
{
inline auto api_cast(engine_application_t engine_app)
{
    return reinterpret_cast<engine::Application*>(engine_app);
}

inline auto api_cast(engine_ui_document_t doc)
{
    return reinterpret_cast<engine::UiDocument*>(doc);
}

inline auto api_cast(engine_ui_data_handle_t handle)
{
    return reinterpret_cast<engine::UiDataHandle*>(handle);
}

inline auto api_cast(engine_scene_t engine_scene_t)
{
    return reinterpret_cast<engine::Scene*>(engine_scene_t);
}

inline auto api_cast(engine_game_object_t go)
{
    return static_cast<entt::entity>(go);
}

inline auto api_cast(engine_component_view_t comp_view)
{
    return reinterpret_cast<entt::runtime_view*>(comp_view);
}

inline auto api_cast(engine_component_iterator_t it)
{
    return reinterpret_cast<decltype(std::declval<entt::runtime_view>().begin())*>(it);
}

inline auto api_cast(const engine_model_desc_t* desc)
{
    return reinterpret_cast<const engine::ModelDesc*>(desc);
}

inline auto api_cast(const engine_model_node_desc_t* desc)
{
    return reinterpret_cast<const engine::ModelNodeDesc*>(desc);
}

inline auto api_cast(const engine::ModelNodeDesc* desc)
{
    return reinterpret_cast<const engine_model_node_desc_t*>(desc);
}

inline auto api_cast(const engine_texture_2d_desc_t* desc)
{
    return reinterpret_cast<const engine::TextureDesc*>(desc);
}

inline auto api_cast(const engine::TextureDesc& desc)
{
    return reinterpret_cast<const engine_texture_2d_desc_t*>(&desc);
}

inline auto api_cast(const engine_geometry_desc_t* desc)
{
    return reinterpret_cast<const engine::GeometryDesc*>(desc);
}

inline auto api_cast(const engine::GeometryDesc& desc)
{
    return reinterpret_cast<const engine_geometry_desc_t*>(&desc);
}

inline auto api_cast(const engine_material_desc_t* desc)
{
    return reinterpret_cast<const engine::MaterialDesc*>(desc);
}

inline auto api_cast(const engine::MaterialDesc& desc)
{
    return reinterpret_cast<const engine_material_desc_t*>(&desc);
}

inline auto api_cast(const engine_skin_desc_t* desc)
{
    return reinterpret_cast<const engine::SkinDesc*>(desc);
}

inline auto api_cast(const engine::SkinDesc& desc)
{
    return reinterpret_cast<const engine_skin_desc_t*>(&desc);
}

inline auto api_cast(const engine_collision_contact_point_desc_t* contact)
{
    return reinterpret_cast<const engine::CollisionContactPointDesc*>(contact);
}

inline auto api_cast(const engine::CollisionContactPointDesc& contact)
{
    return reinterpret_cast<const engine_collision_contact_point_desc_t*>(&contact);
}

inline auto api_cast(const engine_collision_desc_t* contact)
{
    return reinterpret_cast<const engine::CollisionDesc*>(contact);
}
inline auto api_cast(const engine::CollisionDesc& contact)
{
    return reinterpret_cast<const engine_collision_desc_t*>(&contact);
}

inline auto api_cast(const engine_skin_t* contact)
{
    return reinterpret_cast<const engine::Skin*>(contact);
}

inline auto api_cast(engine_skin_t* contact)
{
    return reinterpret_cast<engine::Skin*>(contact);
}

inline auto api_cast(const engine::Skin& contact)
{
    return reinterpret_cast<const engine_skin_t*>(&contact);
}

inline auto api_cast(engine::Skin& contact)
{
    return reinterpret_cast<engine_skin_t*>(&contact);
}

template<typename T>
inline T add_component(engine_scene_t scene, engine_game_object_t engine_game_object_t)
{
    auto sc = api_cast(scene);
    auto entity = api_cast(engine_game_object_t);
    auto ret = sc->add_component<T>(entity);
    return *ret;
}

template<typename T>
inline T get_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = api_cast(scene);
    auto entity = api_cast(game_object);
    return *sc->get_component<T>(entity);
}

template<typename T>
inline void update_component(engine_scene_t scene, engine_game_object_t game_object, const T* comp)
{
    auto sc = api_cast(scene);
    auto entity = api_cast(game_object);
    sc->update_component<T>(entity, *comp);
}

template<typename T>
inline void remove_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = api_cast(scene);
    auto entity = api_cast(game_object);
    sc->remove_component<T>(entity);
}

template<typename T>
inline bool has_component(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = api_cast(scene);
    auto entity = api_cast(game_object);
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

bool engineApplicationIsEditorEnabled(engine_application_t handle)
{
    return dynamic_cast<engine::ApplicationEditor*>(api_cast(handle)) != nullptr;
}

void engineApplicationDestroy(engine_application_t handle)
{
	auto* app = api_cast(handle);
	delete app;
}

bool engineApplicationIsKeyboardButtonDown(engine_application_t handle, engine_keyboard_keys_t key)
{
	auto* app = api_cast(handle);
    if (!app->is_keyboard_enabled())
    {
        return false;
    }
	return app->keyboard_is_key_down(key);
}

bool engineApplicationIsKeyboardButtonUp(engine_application_t handle, engine_keyboard_keys_t key)
{
	return !engineApplicationIsKeyboardButtonDown(handle, key);
}

engine_fvec2_t engineApplicationGetMouseCoords(engine_application_t handle)
{
	auto* app = api_cast(handle);
    if (!app->is_mouse_enabled())
    {
        return {};
    }
	return app->mouse_get_coords();
}

bool engineApplicationIsMouseButtonDown(engine_application_t handle, engine_mouse_button_t button)
{
	auto* app = api_cast(handle);
    if (!app->is_mouse_enabled())
    {
        return false;
    }
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
    auto* app = api_cast(handle);
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
	auto* app = api_cast(handle);
	return app->begine_frame();
}

engine_result_code_t engineApplicationFrameSceneUpdate(engine_application_t handle, engine_scene_t scene, float delta_time)
{
    if (!handle && !scene)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
	auto* app = api_cast(handle);
	auto* scene_typed = api_cast(scene);
	return app->update_scene(scene_typed, delta_time);
}

engine_application_frame_end_info_t engineApplicationFrameEnd(engine_application_t handle)
{
	auto* app = api_cast(handle);
	return app->end_frame();
}

engine_result_code_t engineApplicationCreateShader(engine_application_t handle, const engine_shader_create_desc_t* desc, const char* name, engine_shader_t* out)
{
    if (!handle || !desc || !name || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(handle);

    std::vector<std::string> vertex_shaders;
    auto ptr = desc->vertex_shader_filenames;
    while (*ptr != nullptr)
    {
        vertex_shaders.push_back(*ptr);
        ptr++;
    }

    std::vector<std::string> fragment_shaders;
    ptr = desc->fragment_shader_filenames;
    while (*ptr != nullptr)
    {
        fragment_shaders.push_back(*ptr);
        ptr++;
    }
    const auto ret = app->add_shader(vertex_shaders, fragment_shaders, name);
    *out = ret;
    engineLog(fmt::format("Created shader: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_shader_t engineApplicationGetShaderByName(engine_application_t handle, const char* name)
{
    const auto* app = api_cast(handle);
    return app->get_shader(name);
}

void engineApplicationDestroyShader(engine_application_t handle, engine_shader_t shader)
{
    if (handle)
    {       
        auto* app = api_cast(handle);
        app->destroy_shader(shader);  
    }
}

engine_result_code_t engineApplicationCreateFontFromFile(engine_application_t handle, const char* file_name, const char* handle_name)
{
    auto* app = api_cast(handle);
    const auto result = app->add_font_from_file(file_name, handle_name);
    return result ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engineApplicationCreateGeometryFromDesc(engine_application_t handle, const engine_geometry_desc_t* desc, engine_geometry_t* out)
{
    auto* app = api_cast(handle);
    const auto geo_desc = api_cast(desc);
    const auto ret = app->add_geometry(geo_desc->vertex_laytout, geo_desc->vertex_count, geo_desc->vertex_data, geo_desc->indicies, geo_desc->name);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created geometry: {}, with id: {}\n", geo_desc->name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_geometry_t engineApplicationGetGeometryByName(engine_application_t handle, const char* name)
{
    const auto* app = api_cast(handle);
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
    const auto* app = api_cast(handle);
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

void engineApplicationDestroyGeometry(engine_application_t handle, engine_geometry_t geometry)
{
    assert(handle);
    api_cast(handle)->destroy_geometry(geometry);
}

engine_result_code_t engineApplicationCreateTexture2DFromDesc(engine_application_t handle, const engine_texture_2d_desc_t* desc, engine_texture2d_t* out)
{
    auto* app = api_cast(handle);
    const auto typed_desc = api_cast(desc);
    const auto ret = app->add_texture(*typed_desc);

    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created texture: {}, with id: {}\n", typed_desc->name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationCreateTexture2DFromFile(engine_application_t handle, const char* file_name, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out)
{
    auto* app = api_cast(handle);
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
    const auto* app = api_cast(handle);
    return app->get_texture(name);
}

void engineApplicationDestroyTexture2D(engine_application_t handle, engine_texture2d_t tex2d)
{
    assert(handle);
    api_cast(handle)->destroy_texture(tex2d);
}

bool engineApplicationDoTexture2DNameExists(engine_application_t handle, const char* name)
{
    assert(handle);
    const auto* app = api_cast(handle);
    return app->get_texture(name) != ENGINE_INVALID_OBJECT_HANDLE;
}

engine_result_code_t engineApplicationAllocateModelDescAndLoadDataFromFile(engine_application_t handle, engine_model_specification_t spec, const char* file_name, const char* base_dir, engine_model_desc_t** out)
{
    if (!handle || !out || !file_name || !base_dir)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    assert(*out == nullptr && "Model desc handle must be nullptr before calling this function!");
    assert(spec == ENGINE_MODEL_SPECIFICATION_GLTF_2);
    const auto assets_dir = engine::AssetStore::get_instance().get_models_base_path() / base_dir;
    const auto file_data = engine::AssetStore::get_instance().get_raw_data_content(assets_dir / file_name);
    if (file_data.get_size() == 0)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    const auto model_info = new engine::ModelDesc(engine::parse_gltf_data_from_memory({ file_data.get_data_ptr(), file_data.get_size() }, assets_dir.string()));
    *out = reinterpret_cast<engine_model_desc_t*>(model_info);
    return ENGINE_RESULT_CODE_OK;
}

void engineApplicationReleaseModelDesc(engine_application_t handle, engine_model_desc_t* model_info)
{
    auto* app = api_cast(handle);
    auto typed_desc = api_cast(model_info);
    delete typed_desc;
}

engine_result_code_t engineApplicationSceneCreate(engine_application_t handle, engine_scene_create_desc_t desc, engine_scene_t* out)
{
    if (!handle)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(handle);
    *out = reinterpret_cast<engine_scene_t>(app->allocate_scene(desc));
    if (!out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
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
        auto* app = api_cast(handle);
        app->release_scene(api_cast(scene));
    }
}


engine_game_object_t engineSceneCreateGameObject(engine_scene_t scene)
{
    auto sc = api_cast(scene);
    auto new_entity = sc->create_new_entity();
    return static_cast<engine_game_object_t>(new_entity);
}

void engineSceneDestroyGameObject(engine_scene_t scene, engine_game_object_t game_object)
{
    auto sc = api_cast(scene);
    sc->destroy_entity(api_cast(game_object));
}

void engineScenePhysicsSetGravityVector(engine_scene_t scene, const float gravity[3])
{
    auto sc = api_cast(scene);
    sc->set_physcis_gravity(std::array<float, 3>{gravity[0], gravity[1], gravity[2]});
}

size_t engineScenePhysicsGetNumCollisions(engine_scene_t scene)
{
    auto sc = api_cast(scene);
    return sc->get_physcis_collisions().size();
}

const engine_collision_desc_t* engineScenePhysicsGetCollisionDesc(engine_scene_t scene, size_t idx)
{
    auto sc = api_cast(scene);
    return api_cast(sc->get_physcis_collisions().at(idx));
}

engine_ray_hit_info_t engineScenePhysicsRayCast(engine_scene_t scene, const engine_game_object_t* ignore_list, size_t ignore_list_count, const engine_ray_t* ray, float max_distance)
{
    auto sc = api_cast(scene);
    return sc->raycast_into_physics_world(*ray, { ignore_list, ignore_list_count }, max_distance);
}

bool engineScenePhysicsAddForce(engine_scene_t scene, engine_game_object_t go, const float force[3], engine_force_type_t type)
{
    auto sc = api_cast(scene);
    auto entity = api_cast(go);
    const auto result = sc->add_force_to_physics_entity(entity, std::array<float, 3>{force[0], force[1], force[2]}, type);
    assert(result && "Failed to add force to physics entity!");
    return result;
}

engine_result_code_t engineApplicationCreateUiDocumentDataHandle(engine_application_t app, const char* name, const engine_ui_document_data_binding_t* bindings, size_t bindings_count, engine_ui_data_handle_t* out)
{
    if (bindings_count == 0 && !bindings)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }

    if (app && name && out)
    {
        auto* app_handle = api_cast(app);
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
        auto* data_handle = api_cast(handle);
        delete data_handle;
    }
}

void engineUiDataHandleDirtyAllVariables(engine_ui_data_handle_t handle)
{
    if (handle)
    {
        auto* data_handle = api_cast(handle);
        data_handle->dirty_all_variables();
    }
}

void engineUiDataHandleDirtyVariable(engine_ui_data_handle_t handle, const char* name)
{
    if (handle)
    {
        auto* data_handle = api_cast(handle);
        data_handle->dirty_variable(name);
    }
}

engine_result_code_t engineApplicationCreateUiDocumentFromFile(engine_application_t app, const char* file_path, engine_ui_document_t* out)
{
    if (app && file_path && out)
    {
        auto* app_handle = api_cast(app);
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
        auto* doc_handle = api_cast(doc);
        delete doc_handle;  
    }
}

void engineUiDocumentShow(engine_ui_document_t ui_doc)
{
    if (ui_doc)
    {
        auto* doc = api_cast(ui_doc);
        doc->show();
    }
}

void engineUiDocumentHide(engine_ui_document_t ui_doc)
{
    if (ui_doc)
    {
        auto* doc = api_cast(ui_doc);
        doc->hide();
    }
}

engine_result_code_t engineUiDocumentGetElementById(engine_ui_document_t document, const char* id, engine_ui_element_t* out)
{
    static std::map<std::string, engine::UiElement> ui_elements_cache;
    engine_result_code_t ret = ENGINE_RESULT_CODE_FAIL;
    if (document && id && out)
    {
        auto doc = api_cast(document);
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
        auto rv = api_cast(view);
        delete rv;
    }
}

engine_result_code_t engineComponentViewCreateBeginComponentIterator(engine_component_view_t view, engine_component_iterator_t* out)
{
    if (view && out)
    {
        auto rv = api_cast(view);
        *out = reinterpret_cast<engine_component_iterator_t>(new decltype(rv->begin())(rv->begin()));
        return ENGINE_RESULT_CODE_OK;
    }
    return ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engineComponentViewCreateEndComponentIterator(engine_component_view_t view, engine_component_iterator_t* out)
{
    if (view && out)
    {
        auto rv = api_cast(view);
        *out = reinterpret_cast<engine_component_iterator_t>(new decltype(rv->end())(rv->end()));
        return ENGINE_RESULT_CODE_OK;
    }
    return ENGINE_RESULT_CODE_FAIL;
}

void engineComponentIteratorNext(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it_typed = api_cast(iterator);
        (*it_typed)++;
    }
}

engine_game_object_t engineComponentIteratorGetGameObject(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it_typed = api_cast(iterator);
        return static_cast<engine_game_object_t>(**it_typed);
    }
    return ENGINE_INVALID_GAME_OBJECT_ID;
}

bool engineComponentIteratorCheckEqual(engine_component_iterator_t lhs, engine_component_iterator_t rhs)
{
    bool ret = false;
    if (lhs && rhs)
    {
        auto lhs_typed = api_cast(lhs);
        auto rhs_typed = api_cast(rhs);
        ret = (*lhs_typed == *rhs_typed);
    }
    return ret;
}

void engineDeleteComponentIterator(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it = api_cast(iterator);
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
    auto sc = api_cast(scene);
    auto rv = api_cast(view);
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

engine_skinned_mesh_component_t engineSceneAddSkinnedMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_skinned_mesh_component_t>(scene, game_object);
}

engine_skinned_mesh_component_t engineSceneGetSkinnedMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_skinned_mesh_component_t>(scene, game_object);
}

void engineSceneUpdateSkinnedMeshComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_skinned_mesh_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveSkinnedMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_skinned_mesh_component_t>(scene, game_object);
}

bool engineSceneHasSkinnedMeshComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_skinned_mesh_component_t>(scene, game_object);
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

engine_sprite_component_t engineSceneAddSpriteComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return add_component<engine_sprite_component_t>(scene, game_object);
}

engine_sprite_component_t engineSceneGetSpriteComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_sprite_component_t>(scene, game_object);
}

void engineSceneUpdateSpriteComponent(engine_scene_t scene, engine_game_object_t game_object, const engine_sprite_component_t* comp)
{
    update_component(scene, game_object, comp);
}

void engineSceneRemoveSpriteComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    remove_component<engine_sprite_component_t>(scene, game_object);
}

bool engineSceneHasSpriteComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_sprite_component_t>(scene, game_object);
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
    auto sc = api_cast(scene);
    auto rv = api_cast(view);
    sc->attach_component_to_runtime_view<engine_camera_component_t>(*rv);
}

engine_fvec3_t engineSceneCameraComponentConvertWorldPositionToScreenPosition(engine_scene_t scene, engine_game_object_t game_object, const float world_pos[3])
{
    assert(has_component<engine_camera_component_t>(scene, game_object));
    auto sc = api_cast(scene);
    const auto coords = sc->convert_world_point_to_screen_point({ world_pos[0], world_pos[1], world_pos[2] }, game_object);
    engine_fvec3_t ret{};
    ret.x = coords.x;
    ret.y = coords.y;
    ret.z = coords.z;
    return ret;
}

engine_fvec3_t engineSceneCameraComponentConvertSpacePositionToWorldPosition(engine_scene_t scene, engine_game_object_t game_object, const engine_fvec3_t screen_position)
{
    assert(has_component<engine_camera_component_t>(scene, game_object));
    auto sc = api_cast(scene);
    engine_fvec3_t ret{};
    const auto coords = sc->convert_screen_point_to_world_point({ screen_position.x, screen_position.y, screen_position.z }, game_object);
    ret.x = coords.x;
    ret.y = coords.y;
    ret.z = coords.z;
    return ret;
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

engine_children_component_t engineSceneGetChildrenComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return get_component<engine_children_component_t>(scene, game_object);
}

bool engineSceneHasChildrenComponent(engine_scene_t scene, engine_game_object_t game_object)
{
    return has_component<engine_children_component_t>(scene, game_object);
}

engine_fvec3_t engineCollisionContactPointDescGetPointOnObjectA(const engine_collision_contact_point_desc_t* contact)
{
    assert(contact);
    const auto typed_contact = api_cast(contact);
    if (typed_contact)
    {
        return { typed_contact->point_on_obj_a.x, typed_contact->point_on_obj_a.y, typed_contact->point_on_obj_a.z };
    }
    return { 0.0f, 0.0f, 0.0f };
}

engine_fvec3_t engineCollisionContactPointDescGetPointOnObjectB(const engine_collision_contact_point_desc_t* contact)
{
    assert(contact);
    const auto typed_contact = api_cast(contact);
    if (typed_contact)
    {
        return { typed_contact->point_on_obj_b.x, typed_contact->point_on_obj_b.y, typed_contact->point_on_obj_b.z };
    }
    return { 0.0f, 0.0f, 0.0f };
}

int32_t engineCollisionContactPointDescGetLifetime(const engine_collision_contact_point_desc_t* contact)
{
    assert(contact);
    return api_cast(contact)->lifetime;
}

engine_game_object_t engineCollisionDescGetObjectA(const engine_collision_desc_t* desc)
{
    assert(desc);
    return static_cast<engine_game_object_t>(api_cast(desc)->object_a);
}

engine_game_object_t engineCollisionDescGetObjectB(const engine_collision_desc_t* desc)
{
    assert(desc);
    return static_cast<engine_game_object_t>(api_cast(desc)->object_b);
}

size_t engineCollisionDescGetContactPointsCount(const engine_collision_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->contact_points.size();
}

const engine_collision_contact_point_desc_t* engineCollisionDescGetContactPoint(const engine_collision_desc_t* desc, size_t idx)
{
    assert(desc);
    auto typed_desc = api_cast(desc);
    if (idx < typed_desc->contact_points.size())
    {
        auto ret = api_cast(typed_desc->contact_points.at(idx));
        return ret;
    }
    assert("Index out of bounds for contact points array!");
    return nullptr;
}

const char* engineTexture2dDescGetName(const engine_texture_2d_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

uint32_t engineTexture2dDescGetWidth(const engine_texture_2d_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->width;
}

uint32_t engineTexture2dDescGetHeight(const engine_texture_2d_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->height;
}

engine_data_layout_t engineTexture2dDescGetDataLayout(const engine_texture_2d_desc_t* desc)
{
    using namespace engine;
    const auto data_layout = [](const auto engine_api_layout)
        {
            switch (engine_api_layout)
            {
            case DataLayout::eRGBA_FP32: return ENGINE_DATA_LAYOUT_RGBA_FP32;
            case DataLayout::eR_FP32: return ENGINE_DATA_LAYOUT_R_FP32;

            case DataLayout::eRGBA_U8: return ENGINE_DATA_LAYOUT_RGBA_U8;
            case DataLayout::eRGB_U8: return ENGINE_DATA_LAYOUT_RGB_U8;
            case DataLayout::eR_U8: return ENGINE_DATA_LAYOUT_R_U8;

            default:
                return ENGINE_DATA_LAYOUT_COUNT;
            }
        }(api_cast(desc)->layout);
    return data_layout;
}

const void* engineTexture2dDescGetData(const engine_texture_2d_desc_t* desc)
{
    return api_cast(desc)->data.data();
}

const char* engineGeometryDescGetName(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

const void* engineGeometryDescGetVertsData(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->vertex_data.data();
}

size_t engineGeometryDescGetVertsDataSize(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->vertex_data.size();
}

uint32_t engineGeometryDescGetVertsCount(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->vertex_count;
}

const uint32_t* engineGeometryDescGetIndsData(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->indicies.data();
}

uint32_t engineGeometryDescGetIndsCount(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->indicies.size();
}

engine_vertex_attributes_layout_t engineGeometryDescGetAttributesLayout(const engine_geometry_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->vertex_laytout;
}

const char* engineMaterialDescGetName(const engine_material_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

engine_color_desc_t engineMaterialDescGetDiffuseColor(const engine_material_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    // memcpy?
    engine_color_desc_t ret{};
    ret.r = typed_desc->diffuse_factor.r;
    ret.g = typed_desc->diffuse_factor.g;
    ret.b = typed_desc->diffuse_factor.b;
    ret.a = typed_desc->diffuse_factor.a;
    return ret;
}

uint32_t engineMaterialDescGetDiffuseTextureIndex(const engine_material_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->diffuse_texture;
}

const char* engineSkinDescGetName(const engine_skin_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

uint32_t engineSkinDescGetJointsCount(const engine_skin_desc_t* desc)
{
    assert(desc);
    return static_cast<uint32_t>(api_cast(desc)->inverse_bind_matrix_map.size());
}

uint32_t engineSkinDescGetJointIndex(const engine_skin_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    // get key value from map
    if (idx < typed_desc->inverse_bind_matrix_map.size())
    {
        auto it = typed_desc->inverse_bind_matrix_map.begin();
        std::advance(it, idx);
        return it->first;
    }
    return ENGINE_INVALID_OBJECT_HANDLE;
}

const char* engineModelNodeDescGetName(const engine_model_node_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

const engine_model_node_desc_t* engineModelNodeDescGetParent(const engine_model_node_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    if (typed_desc->parent)
    {
        return api_cast(typed_desc->parent);
    }
    return nullptr;
}

const engine_model_node_desc_t* engineModelNodeDescGetChildren(const engine_model_node_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    if (idx < typed_desc->children.size())
    {
        return api_cast(typed_desc->children.at(idx));
    }
    assert("Index out of bounds for children array!");
    return nullptr;
}

uint32_t engineModelNodeDescGetChildrenCount(const engine_model_node_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return static_cast<uint32_t>(typed_desc->children.size());
}

uint32_t engineModelNodeDescGetIndex(const engine_model_node_desc_t* desc)
{
    return api_cast(desc)->index;
}

uint32_t engineModelNodeDescGetGeometryIndex(const engine_model_node_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->mesh;
}

uint32_t engineModelNodeDescGetSkinIndex(const engine_model_node_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->skin;
}

uint32_t engineModelNodeDescGetMaterialIndex(const engine_model_node_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->material;
}

engine_fvec3_t engineModelNodeDescGetTranslation(const engine_model_node_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    engine_fvec3_t ret{};
    ret.x = typed_desc->translation[0];
    ret.y = typed_desc->translation[1];
    ret.z = typed_desc->translation[2];
    return ret;
}

engine_fvec3_t engineModelNodeDescGetScale(const engine_model_node_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    engine_fvec3_t ret{};
    ret.x = typed_desc->scale[0];
    ret.y = typed_desc->scale[1];
    ret.z = typed_desc->scale[2];
    return ret;
}

engine_fvec4_t engineModelNodeDescGetRotationQuaternion(const engine_model_node_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    engine_fvec4_t ret{};
    ret.x = typed_desc->rotation[0];
    ret.y = typed_desc->rotation[1];
    ret.z = typed_desc->rotation[2];
    ret.w = typed_desc->rotation[3];
    return ret;
}

const engine_model_node_desc_t* engineModelDescGetNodeDesc(const engine_model_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->nodes.at(idx).get());
}

uint32_t engineModelDescGetNodesDescCount(const engine_model_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->nodes.size();
}

const engine_texture_2d_desc_t* engineModelDescGetTexture2dDesc(const engine_model_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->textures.at(idx));
}

uint32_t engineModelDescGetTextures2dDescCount(const engine_model_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->textures.size();
}

const engine_geometry_desc_t* engineModelDescGetGeometryDesc(const engine_model_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->geometries.at(idx));
}

uint32_t engineModelDescGetGeometriesDescCount(const engine_model_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->geometries.size();
}

const engine_material_desc_t* engineModelDescGetMaterialDesc(const engine_model_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->materials.at(idx));
}

uint32_t engineModelDescGetMaterialsDescCount(const engine_model_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->materials.size();
}

const engine_skin_desc_t* engineModelDescGetSkinDesc(const engine_model_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->skins.at(idx));
}

uint32_t engineModelDescGetSkinsDescCount(const engine_model_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->skins.size();
}

engine_skin_t* engineApplicationCreateSkinFromDesc(engine_application_t handle, const engine_skin_desc_t* desc, const engine_model_node_desc_t* root)
{
    if (!handle || !desc || !root)
    {
        return nullptr;
    }
    auto* app = api_cast(handle);
    const auto typed_desc = api_cast(desc);
    const auto typed_root = api_cast(root);
    return reinterpret_cast<engine_skin_t*>(new engine::Skin(*typed_desc, *typed_root));
}

void engineApplicationDestroySkin(engine_application_t handle, engine_skin_t* skin)
{
    if (!handle || !skin)
    {
        return;
    }
    auto* app = api_cast(handle);
    auto typed_skin = api_cast(skin);
    delete typed_skin;
}

const char* engineSkinGetName(const engine_skin_t* skin)
{
    if (!skin)
    {
        return nullptr;
    }
    return api_cast(skin)->get_name().c_str();
}
