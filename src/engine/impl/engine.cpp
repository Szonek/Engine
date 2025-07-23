#include <engine.h>

#include "application.h"
#include "application_editor.h"
#include "scene.h"
#include "asset_store.h"
#include "ui_document.h"
#include "collision_desc.h"
#include "gltf_parser.h"
#include "skin.h"
#include "animation_controller.h"
#include "profiler.h"

#include "logger.h"

#include <utility>

#include <format>

namespace
{
    static engine_application_t G_ACTIVE_APP = nullptr;
    static engine_scene_t G_ACTIVE_SCENE = nullptr;
}

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

inline auto api_cast(engine_animation_desc_t* desc)
{
    return reinterpret_cast<engine::AnimationClipDesc*>(desc);
}

inline auto api_cast(const engine_animation_desc_t* desc)
{
    return reinterpret_cast<const engine::AnimationClipDesc*>(desc);
}

inline auto api_cast(engine::AnimationClipDesc& desc)
{
    return reinterpret_cast<engine_animation_desc_t*>(&desc);
}

inline auto api_cast(const engine::AnimationClipDesc& desc)
{
    return reinterpret_cast<const engine_animation_desc_t*>(&desc);
}

inline auto api_cast(engine_animation_controller_t* controller)
{
    return reinterpret_cast<engine::AnimationController*>(controller);
}

inline auto api_cast(const engine_animation_controller_t* controller)
{
    return reinterpret_cast<const engine::AnimationController*>(controller);
}

inline auto api_cast(engine::AnimationController& contact)
{
    return reinterpret_cast<engine_animation_controller_t*>(&contact);
}

inline auto api_cast(const engine::AnimationController& contact)
{
    return reinterpret_cast<const engine_animation_controller_t*>(&contact);
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

engine_profiler_ctx_t* engineProfileZoneStart(const char* name, bool capture_call_stack)
{
    TracyCZoneCtx* ret = new TracyCZoneCtx;
    ENGINE_PROFILE_ZONE_CONTEXT_START(context, "engine_api", capture_call_stack);
    TracyCZoneName(context, name, std::strlen(name));
    *ret = context;
    return reinterpret_cast<engine_profiler_ctx_t*>(ret);
}

void engineProfileZoneEnd(engine_profiler_ctx_t* ctx)
{
    auto tracy_ctx = reinterpret_cast<TracyCZoneCtx*>(ctx);
    ENGINE_PROFILE_ZONE_CONTEXT_END(*tracy_ctx);
    delete tracy_ctx;
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

bool engineEditorIsEnabled()
{
    if (!G_ACTIVE_APP)
    {
        return false;
    }
    return dynamic_cast<engine::ApplicationEditor*>(api_cast(G_ACTIVE_APP)) != nullptr;
}

void engineApplicationDestroy(engine_application_t handle)
{
	auto* app = api_cast(handle);
	delete app;
}

engine_result_code_t engineApplicationSetActive(engine_application_t handle)
{
    G_ACTIVE_APP = handle;
    return ENGINE_RESULT_CODE_OK;
}

bool engineKeyboardIsButtonDown(engine_keyboard_keys_t key)
{
    if (!G_ACTIVE_APP)
    {
        return false;
    }
	auto* app = api_cast(G_ACTIVE_APP);
    if (!app->is_keyboard_enabled())
    {
        return false;
    }
	return app->keyboard_is_key_down(key);
}

bool engineKeyboardIsButtonUp(engine_keyboard_keys_t key)
{
	return !engineKeyboardIsButtonDown(key);
}

engine_fvec2_t engineMouseCoordsGet()
{
    if (!G_ACTIVE_APP)
    {
        return {};
    }
	auto* app = api_cast(G_ACTIVE_APP);
    if (!app->is_mouse_enabled())
    {
        return {};
    }
	return app->mouse_get_coords();
}

bool engineMouseIsButtonDown(engine_mouse_button_t button)
{
    if (!G_ACTIVE_APP)
    {
        return false;
    }
	auto* app = api_cast(G_ACTIVE_APP);
    if (!app->is_mouse_enabled())
    {
        return false;
    }
	return app->mouse_is_button_down(button);
}

bool engineMouseIsButtonUp(engine_mouse_button_t button)
{
	return !engineMouseIsButtonDown(button);
}

//bool engineApplicationGetFingerInfo(engine_application_t handle, engine_fingers_infos_list_t* infos_list)
//{
//    if (!infos_list)
//    {
//        return false;
//    }
//    auto* app = api_cast(handle);
//    const auto finger_list = app->get_finger_info_events();
//    if(finger_list.empty())
//    {
//        std::memset(infos_list->infos, 0, sizeof(engine_fingers_infos_list_t));
//        return false;
//    }
//    std::memcpy(infos_list->infos, finger_list.data(), sizeof(engine_fingers_infos_list_t));
//    return true;
//}

engine_application_frame_begine_info_t engineFrameBegin()
{
    if (!G_ACTIVE_APP)
    {
        return {};
    }
	auto* app = api_cast(G_ACTIVE_APP);
	return app->begine_frame();
}

engine_result_code_t engineFrameSceneUpdate(float delta_time)
{
    if (!G_ACTIVE_APP && !G_ACTIVE_SCENE)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
	auto* app = api_cast(G_ACTIVE_APP);
	auto* scene_typed = api_cast(G_ACTIVE_SCENE);
	return app->update_scene(scene_typed, delta_time);
}

engine_application_frame_end_info_t engineFrameEnd()
{
    if (!G_ACTIVE_APP)
    {
        return {};
    }
	auto* app = api_cast(G_ACTIVE_APP);
	return app->end_frame();
}

engine_result_code_t engineShaderCreate(const engine_shader_create_desc_t* desc, const char* name, engine_shader_t* out)
{
    if (!G_ACTIVE_APP || !desc || !name || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(G_ACTIVE_APP);

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

engine_shader_t engineShaderGetByName(const char* name)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    const auto* app = api_cast(G_ACTIVE_APP);
    return app->get_shader(name);
}

void engineShaderDestroy(engine_shader_t shader)
{
    if (!G_ACTIVE_APP)
    {
        return;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    app->destroy_shader(shader);  
}

engine_result_code_t engineFontCreateFromFile(const char* file_name, const char* handle_name)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    const auto result = app->add_font_from_file(file_name, handle_name);
    return result ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engineGeometryCreateFromDesc(const engine_geometry_desc_t* desc, engine_geometry_t* out)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(G_ACTIVE_APP);
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

engine_geometry_t engineGeometryGetByName(const char* name)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    const auto* app = api_cast(G_ACTIVE_APP);
    return app->get_geometry(name);
}

engine_geometry_attribute_limit_t engineGeometryGetAttributeLimits(engine_geometry_t geometry, engine_vertex_attribute_type_t type)
{
    engine_geometry_attribute_limit_t ret{};
    ret.elements_count = 0;
    if (!G_ACTIVE_APP || geometry == ENGINE_INVALID_OBJECT_HANDLE || type == ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT)
    {
        return ret;
    }
    const auto* app = api_cast(G_ACTIVE_APP);
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

void engineGeometryDestroy(engine_geometry_t geometry)
{
    if (!G_ACTIVE_APP)
    {
        return;
    }
    api_cast(G_ACTIVE_APP)->destroy_geometry(geometry);
}

engine_result_code_t engineTexture2DCreateFromDesc(const engine_texture_2d_desc_t* desc, engine_texture2d_t* out)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(G_ACTIVE_APP);
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

engine_result_code_t engineTexture2DCreateFromFile(const char* file_name, engine_texture_color_space_t color_space, const char* name, engine_texture2d_t* out)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    const auto ret = app->add_texture_from_file(file_name, name, color_space);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created texture from file: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_texture2d_t engineTextured2DGetByName(const char* name)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    const auto* app = api_cast(G_ACTIVE_APP);
    return app->get_texture(name);
}

void engineTexture2DDestroy(engine_texture2d_t tex2d)
{
    if (!G_ACTIVE_APP)
    {
        return;
    }
    api_cast(G_ACTIVE_APP)->destroy_texture(tex2d);
}

engine_result_code_t engineModelDescAllocateAndLoadDataFromFile(engine_model_specification_t spec, const char* file_name, const char* base_dir, engine_model_desc_t** out)
{
    if (!G_ACTIVE_APP || !out || !file_name || !base_dir)
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

void engineModelDescRelease(engine_model_desc_t* model_info)
{
    if (!G_ACTIVE_APP)
    {
        return;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    auto typed_desc = api_cast(model_info);
    delete typed_desc;
}

engine_result_code_t engineSceneCreate(engine_scene_create_desc_t desc, engine_scene_t* out)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    *out = reinterpret_cast<engine_scene_t>(app->allocate_scene(desc));
    if (!out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return ENGINE_RESULT_CODE_OK;
}

void engineSceneDestroy(engine_scene_t scene)
{
    if (!G_ACTIVE_APP || !scene)
    {
        return;
    }
    if (scene)
    {
        auto* app = api_cast(G_ACTIVE_APP);
        app->release_scene(api_cast(scene));
    }
}

engine_result_code_t engineSceneSetActive(engine_scene_t handle)
{
    if (!G_ACTIVE_APP)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    G_ACTIVE_SCENE = handle;
    return ENGINE_RESULT_CODE_OK;
}


engine_game_object_t engineGameObjectCreate()
{
    if (!G_ACTIVE_SCENE)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    auto new_entity = sc->create_new_entity();
    return static_cast<engine_game_object_t>(new_entity);
}

void engineGameObjectDestroy(engine_game_object_t game_object)
{
    if (!G_ACTIVE_SCENE)
    {
        return;
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    sc->destroy_entity(api_cast(game_object));
}

void enginePhysicsSetGravityVector(const float gravity[3])
{
    if (!G_ACTIVE_SCENE)
    {
        return;
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    sc->set_physcis_gravity(std::array<float, 3>{gravity[0], gravity[1], gravity[2]});
}

size_t enginePhysicsGetNumCollisions()
{
    if (!G_ACTIVE_SCENE)
    {
        return 0;
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    return sc->get_physcis_collisions().size();
}

const engine_collision_desc_t* enginePhysicsGetCollisionDesc(size_t idx)
{
    if (!G_ACTIVE_SCENE)
    {
        return nullptr;
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    return api_cast(sc->get_physcis_collisions().at(idx));
}

engine_ray_hit_info_t enginePhysicsRayCast(const engine_game_object_t* ignore_list, size_t ignore_list_count, const engine_ray_t* ray, float max_distance)
{
    if (!G_ACTIVE_SCENE)
    {
        return {};
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    return sc->raycast_into_physics_world(*ray, { ignore_list, ignore_list_count }, max_distance);
}

bool enginePhysicsAddForce(engine_game_object_t go, const float force[3], engine_force_type_t type)
{
    if (!G_ACTIVE_SCENE)
    {
        return false;
    }

    auto sc = api_cast(G_ACTIVE_SCENE);
    auto entity = api_cast(go);
    const auto result = sc->add_force_to_physics_entity(entity, std::array<float, 3>{force[0], force[1], force[2]}, type);
    assert(result && "Failed to add force to physics entity!");
    return result;
}

engine_result_code_t engineUiDocumentCreateDataHandle(const char* name, const engine_ui_document_data_binding_t* bindings, size_t bindings_count, engine_ui_data_handle_t* out)
{
    if (!G_ACTIVE_APP || (bindings_count == 0 && !bindings))
    {
        return ENGINE_RESULT_CODE_FAIL;
    }

    if (name && out)
    {
        auto* app_handle = api_cast(G_ACTIVE_APP);
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

engine_result_code_t engineUiDocumentCreateFromFile(const char* file_path, engine_ui_document_t* out)
{
    if (G_ACTIVE_APP && file_path && out)
    {
        auto* app_handle = api_cast(G_ACTIVE_APP);
        auto* ret = new engine::UiDocument(app_handle->load_ui_document(file_path));
        if (ret)
        {
            *out = reinterpret_cast<engine_ui_document_t>(ret);
            return ENGINE_RESULT_CODE_OK;
        }    
    }
    return ENGINE_RESULT_CODE_FAIL;
}

void engineUiDocumentDestroy(engine_ui_document_t doc)
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

engine_result_code_t engineComponentViewCreate(engine_component_view_t* out)
{
    if (out)
    {
        *out = reinterpret_cast<engine_component_view_t>(new entt::runtime_view());
        return ENGINE_RESULT_CODE_OK;
    }

    return ENGINE_RESULT_CODE_FAIL;
}

void engineComponentViewDestroy(engine_component_view_t view)
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

void engineComponentIteratorDelete(engine_component_iterator_t iterator)
{
    if (iterator)
    {
        auto it = api_cast(iterator);
        delete it;
    }
}

engine_name_component_t engineGameObjectAddNameComponent(engine_game_object_t game_object)
{
    return add_component<engine_name_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_name_component_t engineGameObjectGetNameComponent(engine_game_object_t game_object)
{
    return get_component<engine_name_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateNameComponent(engine_game_object_t game_object, const engine_name_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveNameComponent(engine_game_object_t game_object)
{
    remove_component<engine_name_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasNameComponent(engine_game_object_t game_object)
{
    return has_component<engine_name_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineComponentViewAttachNameComponent(engine_component_view_t view)
{
    auto sc = api_cast(G_ACTIVE_SCENE);
    auto rv = api_cast(view);
    sc->attach_component_to_runtime_view<engine_name_component_t>(*rv);
}

engine_tranform_component_t engineGameObjectAddTransformComponent(engine_game_object_t game_object)
{
    return add_component<engine_tranform_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_tranform_component_t engineGameObjectGetTransformComponent(engine_game_object_t game_object)
{
    return get_component<engine_tranform_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateTransformComponent(engine_game_object_t game_object, const engine_tranform_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveTransformComponent(engine_game_object_t game_object)
{
    remove_component<engine_tranform_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasTransformComponent(engine_game_object_t game_object)
{
    return has_component<engine_tranform_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_mesh_component_t engineGameObjectAddMeshComponent(engine_game_object_t game_object)
{
    return add_component<engine_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_mesh_component_t engineGameObjectGetMeshComponent(engine_game_object_t game_object)
{
    return get_component<engine_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateMeshComponent(engine_game_object_t game_object, const engine_mesh_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveMeshComponent(engine_game_object_t game_object)
{
    remove_component<engine_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasMeshComponent(engine_game_object_t game_object)
{
    return has_component<engine_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_skinned_mesh_component_t engineGameObjectAddSkinnedMeshComponent(engine_game_object_t game_object)
{
    return add_component<engine_skinned_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_skinned_mesh_component_t engineGameObjectGetSkinnedMeshComponent(engine_game_object_t game_object)
{
    return get_component<engine_skinned_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateSkinnedMeshComponent(engine_game_object_t game_object, const engine_skinned_mesh_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveSkinnedMeshComponent(engine_game_object_t game_object)
{
    remove_component<engine_skinned_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasSkinnedMeshComponent(engine_game_object_t game_object)
{
    return has_component<engine_skinned_mesh_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_skin_component_t engineGameObjectAddSkinComponent(engine_game_object_t game_object)
{
    return add_component<engine_skin_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_skin_component_t engineGameObjectGetSkinComponent(engine_game_object_t game_object)
{
    return get_component<engine_skin_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateSkinComponent(engine_game_object_t game_object, const engine_skin_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveSkinComponent(engine_game_object_t game_object)
{
    remove_component<engine_skin_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasSkinComponent(engine_game_object_t game_object)
{
    return has_component<engine_skin_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_joint_attachment_component_t engineGameObjectAddJointAttachmentComponent(engine_game_object_t game_object)
{
    return add_component<engine_joint_attachment_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_joint_attachment_component_t engineGameObjectGetJointAttachmentComponent(engine_game_object_t game_object)
{
    return get_component<engine_joint_attachment_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateJointAttachmentComponent(engine_game_object_t game_object, const engine_joint_attachment_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveJointAttachmentComponent(engine_game_object_t game_object)
{
    remove_component<engine_joint_attachment_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasJointAttachmentComponent(engine_game_object_t game_object)
{
    return has_component<engine_joint_attachment_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_animation_controller_component_t engineGameObjectAddAnimationControllerComponent(engine_game_object_t game_object)
{
    return add_component<engine_animation_controller_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_animation_controller_component_t engineGameObjectGetAnimationControllerComponent(engine_game_object_t game_object)
{
    return get_component<engine_animation_controller_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateAnimationControllerComponent(engine_game_object_t game_object, const engine_animation_controller_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveAnimationControllerComponent(engine_game_object_t game_object)
{
    remove_component<engine_animation_controller_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasAnimationControllerComponent(engine_game_object_t game_object)
{
    return has_component<engine_animation_controller_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_material_component_t engineGameObjectAddMaterialComponent(engine_game_object_t game_object)
{
    return add_component<engine_material_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_material_component_t engineGameObjectGetMaterialComponent(engine_game_object_t game_object)
{
    return get_component<engine_material_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateMaterialComponent(engine_game_object_t game_object, const engine_material_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveMaterialComponent(engine_game_object_t game_object)
{
    remove_component<engine_material_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasMaterialComponent(engine_game_object_t game_object)
{
    return has_component<engine_material_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_light_component_t engineGameObjectAddLightComponent(engine_game_object_t game_object)
{
    return add_component<engine_light_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_light_component_t engineGameObjectGetLightComponent(engine_game_object_t game_object)
{
    return get_component<engine_light_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateLightComponent(engine_game_object_t game_object, const engine_light_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveLightComponent(engine_game_object_t game_object)
{
    remove_component<engine_light_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasLightComponent(engine_game_object_t game_object)
{
    return has_component<engine_light_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_sprite_component_t engineGameObjectAddSpriteComponent(engine_game_object_t game_object)
{
    return add_component<engine_sprite_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_sprite_component_t engineGameObjectGetSpriteComponent(engine_game_object_t game_object)
{
    return get_component<engine_sprite_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateSpriteComponent(engine_game_object_t game_object, const engine_sprite_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveSpriteComponent(engine_game_object_t game_object)
{
    remove_component<engine_sprite_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasSpriteComponent(engine_game_object_t game_object)
{
    return has_component<engine_sprite_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_camera_component_t engineGameObjectAddCameraComponent(engine_game_object_t game_object)
{
    return add_component<engine_camera_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_camera_component_t engineGameObjectGetCameraComponent(engine_game_object_t game_object)
{
    return get_component<engine_camera_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateCameraComponent(engine_game_object_t game_object, const engine_camera_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveCameraComponent(engine_game_object_t game_object)
{
    remove_component<engine_camera_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasCameraComponent(engine_game_object_t game_object)
{
    return has_component<engine_camera_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineComponentViewAttachCameraComponent(engine_component_view_t view)
{
    auto sc = api_cast(G_ACTIVE_SCENE);
    auto rv = api_cast(view);
    sc->attach_component_to_runtime_view<engine_camera_component_t>(*rv);
}

engine_fvec3_t engineCameraComponentConvertWorldPositionToScreenPosition(engine_game_object_t game_object, const float world_pos[3])
{
    assert(has_component<engine_camera_component_t>(G_ACTIVE_SCENE, game_object));
    auto sc = api_cast(G_ACTIVE_SCENE);
    const auto coords = sc->convert_world_point_to_screen_point({ world_pos[0], world_pos[1], world_pos[2] }, game_object);
    engine_fvec3_t ret{};
    ret.x = coords.x;
    ret.y = coords.y;
    ret.z = coords.z;
    return ret;
}

engine_fvec3_t engineCameraComponentConvertSpacePositionToWorldPosition(engine_game_object_t game_object, const engine_fvec3_t screen_position)
{
    assert(has_component<engine_camera_component_t>(G_ACTIVE_SCENE, game_object));
    auto sc = api_cast(G_ACTIVE_SCENE);
    engine_fvec3_t ret{};
    const auto coords = sc->convert_screen_point_to_world_point({ screen_position.x, screen_position.y, screen_position.z }, game_object);
    ret.x = coords.x;
    ret.y = coords.y;
    ret.z = coords.z;
    return ret;
}

engine_rigid_body_component_t engineGameObjectAddRigidBodyComponent(engine_game_object_t game_object)
{
    return add_component<engine_rigid_body_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_rigid_body_component_t engineGameObjectGetRigidBodyComponent(engine_game_object_t game_object)
{
    return get_component<engine_rigid_body_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateRigidBodyComponent(engine_game_object_t game_object, const engine_rigid_body_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveRigidBodyComponent(engine_game_object_t game_object)
{
    remove_component<engine_rigid_body_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasRigidBodyComponent(engine_game_object_t game_object)
{
    return has_component<engine_rigid_body_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_collider_component_t engineGameObjectAddColliderComponent(engine_game_object_t game_object)
{
    return add_component<engine_collider_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_collider_component_t engineGameObjectGetColliderComponent(engine_game_object_t game_object)
{
    return get_component<engine_collider_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateColliderComponent(engine_game_object_t game_object, const engine_collider_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveColliderComponent(engine_game_object_t game_object)
{
    remove_component<engine_collider_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasColliderComponent(engine_game_object_t game_object)
{
    return has_component<engine_collider_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_parent_component_t engineGameObjectAddParentComponent(engine_game_object_t game_object)
{
    return add_component<engine_parent_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_parent_component_t engineGameObjectGetParentComponent(engine_game_object_t game_object)
{
    return get_component<engine_parent_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateParentComponent(engine_game_object_t game_object, const engine_parent_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveParentComponent(engine_game_object_t game_object)
{
    remove_component<engine_parent_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasParentComponent(engine_game_object_t game_object)
{
    return has_component<engine_parent_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_children_component_t engineGameObjectGetChildrenComponent(engine_game_object_t game_object)
{
    return get_component<engine_children_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasChildrenComponent(engine_game_object_t game_object)
{
    return has_component<engine_children_component_t>(G_ACTIVE_SCENE, game_object);
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

const char* engineSkinDescGetJointName(const engine_skin_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    // get key value from map
    if (idx < typed_desc->inverse_bind_matrix_map.size())
    {
        auto it = typed_desc->inverse_bind_matrix_map.begin();
        std::advance(it, idx);
        return it->first.c_str();
    }
    return nullptr;
}

const char* engineAnimationDescGetName(const engine_animation_desc_t* desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
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

const engine_model_node_desc_t* engineModelDescGetNodeDescByName(const engine_model_desc_t* desc, const char* name)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    const auto fnd = std::find_if(typed_desc->nodes.begin(), typed_desc->nodes.end(),
        [&name](const auto& node) {
            return node->name == name;
        });
    if (fnd != typed_desc->nodes.end())
    {
        return api_cast(fnd->get());
    }
    assert("Node with given name not found in model desc!");
    return nullptr;
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

const engine_animation_desc_t* engineModelDescGetAnimationDesc(const engine_model_desc_t* desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    if (idx < typed_desc->animations.size())
    {
        return api_cast(typed_desc->animations.at(idx));
    }
    return nullptr;
}

uint32_t engineModelDescGetAnimationsDescCount(const engine_model_desc_t* desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return static_cast<uint32_t>(typed_desc->animations.size());
}

engine_skin_t* engineCreateSkinFromDesc(const engine_skin_desc_t* desc, const engine_model_node_desc_t* root)
{
    if (!G_ACTIVE_APP || !desc || !root)
    {
        return nullptr;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    const auto typed_desc = api_cast(desc);
    const auto typed_root = api_cast(root);
    return reinterpret_cast<engine_skin_t*>(new engine::Skin(*typed_desc, *typed_root));
}

void engineDestroySkin(engine_skin_t* skin)
{
    if (!G_ACTIVE_APP || !skin)
    {
        return;
    }
    auto* app = api_cast(G_ACTIVE_APP);
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

engine_animation_controller_t* engineCreateAnimationControllerWithSkin(engine_skin_t* skin)
{
    if (!G_ACTIVE_APP || !skin)
    {
        return nullptr;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    auto typed_skin = api_cast(skin);
    auto controller = new engine::AnimationController(typed_skin);
    return api_cast(*controller);
}

void engineDestroyAnimationController(engine_animation_controller_t* controller)
{
    if (!G_ACTIVE_APP || !controller)
    {
        return;
    }
    auto* app = api_cast(G_ACTIVE_APP);
    auto typed_controller = api_cast(controller);
    delete typed_controller;
}

bool engineAnimationControllerAddAnimation(engine_animation_controller_t* controller, const engine_animation_desc_t* desc)
{
    if (!controller || !desc)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    auto typed_desc = api_cast(desc);
    return typed_controller->add_animation(*typed_desc);
}

engine_result_code_t engineAnimationControllerAnimationGetDuration(engine_animation_controller_t* controller, const char* name, float* out)
{
    if (!controller || !name || !out)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }
    auto typed_controller = api_cast(controller);
    *out = typed_controller->get_duration(name);
    if (*out == 0.0f)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return ENGINE_RESULT_CODE_OK;
}

bool engineAnimationControllerAnimationPlay(engine_animation_controller_t* controller, const char* name, size_t layer_id)
{
    if (!controller || !name)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    return typed_controller->play(name, layer_id);
}

bool engineAnimationControllerAnimationCrossFade(engine_animation_controller_t* controller, const char* new_animation_name, size_t layer_id, float duration)
{
    if (!controller || !new_animation_name)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    return typed_controller->cross_fade_to(new_animation_name, layer_id, std::max(0.0f, duration));
}


bool engineAnimationControllerIsAnimationPlaying(engine_animation_controller_t* controller, const char* name)
{
    if (!controller || !name)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);

    return typed_controller->is_playing(name);
}

bool engineAnimationControllerAddLayer(engine_animation_controller_t* controller, size_t id, float default_weight)
{
    if (!controller)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    return typed_controller->add_layer(id, default_weight);
}

bool engineAnimationControllerRemoveLayer(engine_animation_controller_t* controller, size_t id)
{
    if (!controller)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    return typed_controller->remove_layer(id);
}

bool engineAnimationControllerLayerSetWeight(engine_animation_controller_t* controller, size_t id, float new_weight)
{
    if (!controller)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    return typed_controller->set_layer_weight(id, new_weight);
}

bool engineAnimationControllerSetMode(engine_animation_controller_t* controller, size_t id, engine_animation_layer_mode_t mode)
{
    if (!controller)
    {
        return false;
    }
    auto typed_controller = api_cast(controller);
    engine::LayerBlendMode blend_mode = engine::LayerBlendMode::eOverride;
    switch (mode)
    {
    case ENGINE_ANIMATION_LAYER_MODE_OVERRIDE:
    {
        blend_mode = engine::LayerBlendMode::eOverride;
        break;
    }
    case ENGINE_ANIMATION_LAYER_MODE_ADDITIVE:
    {
        blend_mode = engine::LayerBlendMode::eAdditive;
        break;
    }
    default:
    {
        engine::log::log(engine::log::LogLevel::eError, std::format("Unrecognized blend mode: {} for layer id: {}", (std::int32_t)mode, id));
    }
    }
    return typed_controller->set_layer_mode(id, blend_mode);
}

engine_result_code_t engineAnimationControllerAnimationAddEvent(engine_animation_controller_t* controller, const char* anim_name, engine_animation_event_t event, engine_animation_event_id_t* out_id)
{
    if (!controller || !anim_name || !out_id)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }

    auto typed_controller = api_cast(controller);

    const auto [res, ev_id] = typed_controller->add_event(anim_name, event);
    if (!res)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out_id = ev_id;
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineAnimationControllerAnimationRemoveEvent(engine_animation_controller_t* controller, const char* anim_name, engine_animation_event_id_t id)
{
    if (!controller || !anim_name || (id == ENGINE_INVALID_OBJECT_HANDLE))
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }

    auto typed_controller = api_cast(controller);
    const auto res = typed_controller->remove_event(anim_name, id);
    return res ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}
