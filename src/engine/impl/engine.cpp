#include <engine.h>

#include "application.h"
#include "application_editor.h"
#include "scene.h"
#include "nav_mesh.h"
#include "asset_store.h"
#include "ui_document.h"
#include "collision_desc.h"
#include "components_internals/prefab_instance_component.h"
#include "animation_controller.h"
#include "profiler.h"

#include "logger.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <optional>
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

bool engineEditorIsPlaying()
{
    if (!G_ACTIVE_APP)
    {
        return true;
    }
    const auto editor = dynamic_cast<engine::ApplicationEditor*>(api_cast(G_ACTIVE_APP));
    return editor ? editor->is_playing() : true;
}

bool engineEditorIsGameView()
{
    if (!G_ACTIVE_APP)
    {
        return true;
    }
    const auto editor = dynamic_cast<engine::ApplicationEditor*>(api_cast(G_ACTIVE_APP));
    return editor ? editor->is_game_view() : true;
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
	return app->mouse_get_game_coords();
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
    if (ret == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
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
    app->release_shader(shader);
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
    if (!G_ACTIVE_APP || type == ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT)
    {
        return ret;
    }
    const auto* app = api_cast(G_ACTIVE_APP);
    const auto geometry_obj = app->get_geometry(geometry);
    if (!geometry_obj)
    {
        return ret;
    }
    const auto attrib = geometry_obj->get_vertex_attribute(type);
    ret.elements_count = attrib.range_max.size();
    for (auto i = 0; i < ret.elements_count; i++)
    {
        ret.max[i] = attrib.range_max[i];
        ret.min[i] = attrib.range_min[i];
    }
    return ret;
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

void engineTexture2DRelease(engine_texture2d_t tex2d)
{
    if (!G_ACTIVE_APP)
    {
        return;
    }
    api_cast(G_ACTIVE_APP)->release_texture(tex2d);
}

engine_result_code_t enginePrefabLoad(engine_model_specification_t spec, const char* file_name, const char* base_dir, engine_prefab_t* out)
{
    if (!G_ACTIVE_APP || !file_name || !base_dir || !out || spec != ENGINE_MODEL_SPECIFICATION_GLTF_2)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }
    const auto ret = api_cast(G_ACTIVE_APP)->load_prefab(file_name, base_dir);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Loaded prefab: {}, with id: {}\n", file_name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_prefab_t enginePrefabGetByName(const char* name)
{
    if (!G_ACTIVE_APP || !name)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    return api_cast(G_ACTIVE_APP)->get_prefab(std::string_view(name));
}

engine_game_object_t enginePrefabInstantiate(engine_prefab_t prefab)
{
    if (!G_ACTIVE_APP || !G_ACTIVE_SCENE)
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    const auto* typed_prefab = api_cast(G_ACTIVE_APP)->get_prefab(prefab);
    if (!typed_prefab)
    {
        engineLog(fmt::format("Can't instantiate prefab {}: it doesn't exist.\n", prefab).c_str());
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    auto* scene = api_cast(G_ACTIVE_SCENE);
    const auto root = typed_prefab->instantiate(*scene);
    if (static_cast<engine_game_object_t>(root) == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    scene->add_component(root, engine::prefab_instance_internal_component_t(*api_cast(G_ACTIVE_APP), prefab));
    return static_cast<engine_game_object_t>(root);
}

engine_prefab_t enginePrefabInstanceGetPrefab(engine_game_object_t game_object)
{
    if (!G_ACTIVE_SCENE)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    auto* scene = api_cast(G_ACTIVE_SCENE);
    const auto entity = api_cast(game_object);
    if (!scene->is_valid_entity(entity) || !scene->has_component<engine::prefab_instance_internal_component_t>(entity))
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    return scene->get_component<engine::prefab_instance_internal_component_t>(entity)->get_prefab();
}

void enginePrefabRelease(engine_prefab_t prefab)
{
    if (!G_ACTIVE_APP)
    {
        return;
    }
    api_cast(G_ACTIVE_APP)->release_prefab(prefab);
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

bool engineGameObjectIsValid(engine_game_object_t game_object)
{
    if (!G_ACTIVE_SCENE)
    {
        return false;
    }
    auto sc = api_cast(G_ACTIVE_SCENE);
    return sc->is_valid_entity(api_cast(game_object));
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

engine_result_code_t engineNavMeshBuild()
{
    if (!G_ACTIVE_SCENE)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return api_cast(G_ACTIVE_SCENE)->build_nav_mesh();
}

engine_result_code_t engineNavFindPath(const float start[3], const float end[3], const float search_half_extents[3],
    float* out_points, size_t max_points, size_t* out_points_count, engine_nav_path_status_t* out_status)
{
    if (!start || !end || !search_half_extents || !out_points || max_points == 0 || !out_points_count || !out_status)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }
    *out_points_count = 0;
    const auto* nav_mesh = G_ACTIVE_SCENE ? api_cast(G_ACTIVE_SCENE)->get_nav_mesh() : nullptr;
    const auto path = nav_mesh
        ? nav_mesh->find_path(glm::make_vec3(start), glm::make_vec3(end), glm::make_vec3(search_half_extents), max_points)
        : std::nullopt;
    if (!path)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    std::memcpy(out_points, path->points.data(), path->points.size() * sizeof(glm::vec3));
    *out_points_count = path->points.size();
    *out_status = path->partial ? ENGINE_NAV_PATH_STATUS_PARTIAL : ENGINE_NAV_PATH_STATUS_COMPLETE;
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineNavFindNearestPoint(const float point[3], const float search_half_extents[3], float out[3])
{
    if (!point || !search_half_extents || !out)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }
    const auto* nav_mesh = G_ACTIVE_SCENE ? api_cast(G_ACTIVE_SCENE)->get_nav_mesh() : nullptr;
    const auto nearest = nav_mesh ? nav_mesh->find_nearest_point(glm::make_vec3(point), glm::make_vec3(search_half_extents)) : std::nullopt;
    if (!nearest)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    out[0] = nearest->x;
    out[1] = nearest->y;
    out[2] = nearest->z;
    return ENGINE_RESULT_CODE_OK;
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

engine_folder_component_t engineGameObjectAddFolderComponent(engine_game_object_t game_object)
{
    return add_component<engine_folder_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_folder_component_t engineGameObjectGetFolderComponent(engine_game_object_t game_object)
{
    return get_component<engine_folder_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateFolderComponent(engine_game_object_t game_object, const engine_folder_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveFolderComponent(engine_game_object_t game_object)
{
    remove_component<engine_folder_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasFolderComponent(engine_game_object_t game_object)
{
    return has_component<engine_folder_component_t>(G_ACTIVE_SCENE, game_object);
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

engine_nav_surface_component_t engineGameObjectAddNavSurfaceComponent(engine_game_object_t game_object)
{
    return add_component<engine_nav_surface_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_nav_surface_component_t engineGameObjectGetNavSurfaceComponent(engine_game_object_t game_object)
{
    return get_component<engine_nav_surface_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateNavSurfaceComponent(engine_game_object_t game_object, const engine_nav_surface_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveNavSurfaceComponent(engine_game_object_t game_object)
{
    remove_component<engine_nav_surface_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasNavSurfaceComponent(engine_game_object_t game_object)
{
    return has_component<engine_nav_surface_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_nav_modifier_component_t engineGameObjectAddNavModifierComponent(engine_game_object_t game_object)
{
    return add_component<engine_nav_modifier_component_t>(G_ACTIVE_SCENE, game_object);
}

engine_nav_modifier_component_t engineGameObjectGetNavModifierComponent(engine_game_object_t game_object)
{
    return get_component<engine_nav_modifier_component_t>(G_ACTIVE_SCENE, game_object);
}

void engineGameObjectUpdateNavModifierComponent(engine_game_object_t game_object, const engine_nav_modifier_component_t* comp)
{
    update_component(G_ACTIVE_SCENE, game_object, comp);
}

void engineGameObjectRemoveNavModifierComponent(engine_game_object_t game_object)
{
    remove_component<engine_nav_modifier_component_t>(G_ACTIVE_SCENE, game_object);
}

bool engineGameObjectHasNavModifierComponent(engine_game_object_t game_object)
{
    return has_component<engine_nav_modifier_component_t>(G_ACTIVE_SCENE, game_object);
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

size_t engineGameObjectGetChildrenCount(engine_game_object_t game_object)
{
    if (!G_ACTIVE_SCENE)
    {
        return 0;
    }
    return api_cast(G_ACTIVE_SCENE)->get_children(api_cast(game_object)).size();
}

engine_game_object_t engineGameObjectGetChild(engine_game_object_t game_object, size_t index)
{
    if (!G_ACTIVE_SCENE)
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    const auto children = api_cast(G_ACTIVE_SCENE)->get_children(api_cast(game_object));
    return index < children.size() ? static_cast<engine_game_object_t>(children[index]) : ENGINE_INVALID_GAME_OBJECT_ID;
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

engine_skin_t engineSkinGetByName(const char* name)
{
    if (!G_ACTIVE_APP || !name)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    return api_cast(G_ACTIVE_APP)->get_skin(name);
}

engine_animation_set_t engineAnimationSetGetByName(const char* name)
{
    if (!G_ACTIVE_APP || !name)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    return api_cast(G_ACTIVE_APP)->get_animation_set(name);
}

namespace
{
// the controller of the game object's animation controller component in the active scene, or nullptr
engine::AnimationController* find_animation_controller(engine_game_object_t game_object)
{
    if (!G_ACTIVE_SCENE)
    {
        return nullptr;
    }
    return api_cast(G_ACTIVE_SCENE)->get_animation_controller(api_cast(game_object));
}
}  // namespace

bool engineAnimationControllerSetPaused(engine_game_object_t game_object, bool paused)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller)
    {
        return false;
    }
    return typed_controller->set_paused(paused);
}

engine_result_code_t engineAnimationControllerAnimationGetDuration(engine_game_object_t game_object, const char* name, float* out)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !name || !out)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }
    *out = typed_controller->get_duration(name);
    if (*out == 0.0f)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return ENGINE_RESULT_CODE_OK;
}

bool engineAnimationControllerAnimationPlay(engine_game_object_t game_object, const char* name, size_t layer_id)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !name)
    {
        return false;
    }
    return typed_controller->play(name, layer_id);
}

bool engineAnimationControllerAnimationCrossFade(engine_game_object_t game_object, const char* new_animation_name, size_t layer_id, float duration)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !new_animation_name)
    {
        return false;
    }
    return typed_controller->cross_fade_to(new_animation_name, layer_id, std::max(0.0f, duration));
}


bool engineAnimationControllerIsAnimationPlaying(engine_game_object_t game_object, const char* name)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !name)
    {
        return false;
    }

    return typed_controller->is_playing(name);
}

bool engineAnimationControllerIsAnimationPlayingOnLayer(engine_game_object_t game_object, const char* name, size_t layer_id)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !name)
    {
        return false;
    }
    return typed_controller->is_playing(name, layer_id);
}

bool engineAnimationControllerLayerSetWeight(engine_game_object_t game_object, size_t id, float new_weight)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller)
    {
        return false;
    }
    return typed_controller->set_layer_weight(id, new_weight);
}

bool engineAnimationControllerLayerSetPlaybackRate(engine_game_object_t game_object, size_t id, float playback_rate)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller)
    {
        return false;
    }
    return typed_controller->set_layer_playback_rate(id, playback_rate);
}

bool engineAnimationControllerLayerSetLooping(engine_game_object_t game_object, size_t id, bool looping)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller)
    {
        return false;
    }
    return typed_controller->set_layer_looping(id, looping);
}

engine_result_code_t engineAnimationControllerAnimationAddEvent(engine_game_object_t game_object, const char* anim_name, engine_animation_event_t event, engine_animation_event_id_t* out_id)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !anim_name || !out_id)
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }


    const auto [res, ev_id] = typed_controller->add_event(anim_name, event);
    if (!res)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out_id = ev_id;
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineAnimationControllerAnimationRemoveEvent(engine_game_object_t game_object, const char* anim_name, engine_animation_event_id_t id)
{
    auto* typed_controller = find_animation_controller(game_object);
    if (!typed_controller || !anim_name || (id == ENGINE_INVALID_OBJECT_HANDLE))
    {
        return ENGINE_RESULT_CODE_INVALID_ARG;
    }

    const auto res = typed_controller->remove_event(anim_name, id);
    return res ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}
