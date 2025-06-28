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

inline auto api_cast(engine_model_desc2_t desc)
{
    return reinterpret_cast<engine::ModelInfo*>(desc);
}

inline auto api_cast(engine_model_node_desc2_t desc)
{
    return reinterpret_cast<engine::ModelNode*>(desc);
}

inline auto api_cast(engine::ModelNode* desc)
{
    return reinterpret_cast<engine_model_node_desc2_t>(desc);
}

inline auto api_cast(engine_texture_2d_desc2_t desc)
{
    return reinterpret_cast<engine::TextureInfo*>(desc);
}

inline auto api_cast(engine::TextureInfo& desc)
{
    return reinterpret_cast<engine_texture_2d_desc2_t>(&desc);
}

inline auto api_cast(engine_geometry_desc2_t desc)
{
    return reinterpret_cast<engine::GeometryInfo*>(desc);
}

inline auto api_cast(engine::GeometryInfo& desc)
{
    return reinterpret_cast<engine_geometry_desc2_t>(&desc);
}

inline auto api_cast(engine_material_desc2_t desc)
{
    return reinterpret_cast<engine::MaterialInfo*>(desc);
}

inline auto api_cast(engine::MaterialInfo& desc)
{
    return reinterpret_cast<engine_material_desc2_t>(&desc);
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

engine_skin_t engineSceneCreateSkinFromDesc(engine_scene_t scene, const engine_skin_create_desc_t* skin_desc, const engine_model_node_desc_t* nodes)
{
    if (!scene || !skin_desc || !nodes)
    {
        return nullptr;
    }
    if (!skin_desc->name || skin_desc->name[0] == '\0')
    {
        engineLog("Skin name is empty, cannot create skin.\n");
        return nullptr;
    }
    auto* sc = api_cast(scene);
    return nullptr;
}

engine_result_code_t engineApplicationCreateGeometryFromDesc(engine_application_t handle, const engine_geometry_create_desc_t* desc, const char* name, engine_geometry_t* out)
{
    auto* app = api_cast(handle);
    const auto ret = app->add_geometry(desc->verts_layout, desc->verts_count, { reinterpret_cast<const std::byte*>(desc->verts_data), desc->verts_data_size }, { desc->inds, desc->inds_count}, name);
    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created geometry: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationCreateGeometryFromDesc_2(engine_application_t handle, const engine_geometry_desc2_t desc, engine_geometry_t* out)
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

engine_result_code_t engineApplicationCreateTexture2DFromDesc(engine_application_t handle, const engine_texture_2d_create_desc_t* info, const char* name, engine_texture2d_t* out)
{
    auto* app = api_cast(handle);
    const auto ret =  app->add_texture(*info, name);

    if (ret == ENGINE_INVALID_OBJECT_HANDLE || !out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    *out = ret;
    engineLog(fmt::format("Created texture: {}, with id: {}\n", name, ret).c_str());
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationCreateTexture2DFromDesc_2(engine_application_t handle, const engine_texture_2d_desc2_t desc, engine_texture2d_t* out)
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

engine_result_code_t engineApplicationAllocateModelDescAndLoadDataFromFile(engine_application_t handle, engine_model_specification_t spec, const char *file_name, const char* base_dir, engine_model_desc_t* out)
{
    if (!out)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    auto* app = api_cast(handle);
    auto model_info = new engine::ModelInfo(app->load_model_desc_from_file(spec, file_name, base_dir));

    out->internal_handle = reinterpret_cast<const void*>(model_info);

    out->nodes_count = static_cast<std::uint32_t>(model_info->nodes.size());
    if (out->nodes_count > 0)
    {
        out->nodes_array = new engine_model_node_desc_t[out->nodes_count];
        for (std::size_t i = 0; i < out->nodes_count; i++)
        {
            auto copy_arr = [](float* const arr, const auto& glm_vec)
                {
                    for (auto i = 0; i < glm_vec.length(); i++)
                    {
                        arr[i] = glm_vec[i];
                    }
                };

            const auto& in_n = model_info->nodes.at(i);
            auto& ret_n = out->nodes_array[i];
            ret_n.index = in_n->index;
            ret_n.geometry_index = in_n->mesh;
            ret_n.skin_index = in_n->skin;
            ret_n.name = in_n->name.c_str();
            if (ret_n.geometry_index != -1)
            {
                ret_n.material_index = in_n->material;
            }
            else
            {
                ret_n.material_index = ENGINE_INVALID_OBJECT_HANDLE;
            }

            copy_arr(ret_n.translate, in_n->translation);
            copy_arr(ret_n.rotation_quaternion, in_n->rotation);
            copy_arr(ret_n.scale, in_n->scale);

            if (in_n->parent)
            {
                ret_n.parent = &out->nodes_array[in_n->parent->index];
            }
            else
            {
                ret_n.parent = nullptr;
            }
            if (!in_n->children.empty())
            {
                ret_n.children_count = static_cast<std::uint32_t>(in_n->children.size());
                ret_n.children = new engine_model_node_desc_t*[ret_n.children_count];
                for (std::size_t j = 0; j < in_n->children.size(); j++)
                {
                    ret_n.children[j] = &out->nodes_array[in_n->children[j]->index];
                }
            }
            else
            {
                ret_n.children_count = 0;
                ret_n.children = nullptr;
            }
        }
    }

    out->geometries_count = static_cast<std::uint32_t>(model_info->geometries.size());
    if (out->geometries_count > 0)
    {
        out->geometries_array = new engine_geometry_create_desc_t[out->geometries_count];
        out->geometires_name_array = new const char* [out->geometries_count];
        for (std::size_t i = 0; i < out->geometries_count; i++)
        {
            const auto& int_g = model_info->geometries[i];
            auto& ret_g = out->geometries_array[i];

            ret_g.inds_count = int_g.indicies.size();
            ret_g.inds = int_g.indicies.data();

            ret_g.verts_data_size = int_g.vertex_data.size();
            ret_g.verts_data = int_g.vertex_data.data();
            ret_g.verts_layout = int_g.vertex_laytout;
            ret_g.verts_count = int_g.vertex_count;

            out->geometires_name_array[i] = int_g.name.c_str();
        }

    }

    out->textures_count = static_cast<std::uint32_t>(model_info->textures.size());
    if (out->textures_count > 0)
    {
        out->textures_array = new engine_texture_2d_create_desc_t[out->textures_count];
        out->textures_name_array = new const char* [out->textures_count];
        for (std::size_t i = 0; i < out->textures_count; i++)
        {
            const auto& int_m = model_info->textures[i];
            auto& ret_m = out->textures_array[i];

            ret_m.width = int_m.width;
            ret_m.height = int_m.height;
            ret_m.data_layout = int_m.layout;
            ret_m.data = int_m.data.data();

            out->textures_name_array[i] = int_m.name.c_str();
        }
    }

    out->materials_count = static_cast<std::uint32_t>(model_info->materials.size());
    if (out->materials_count > 0)
    {
        out->materials_array = new engine_model_material_desc_t[out->materials_count];
        out->materials_name_array = new const char* [out->materials_count];

        for (std::size_t i = 0; i < out->materials_count; i++)
        {
            const auto& int_m = model_info->materials[i];
            auto& ret_m = out->materials_array[i];

            ret_m.name = int_m.name.c_str();
            
            std::memcpy(ret_m.diffuse_color, glm::value_ptr(int_m.diffuse_factor), sizeof(int_m.diffuse_factor));
            ret_m.diffuse_texture_index = int_m.diffuse_texture;

            out->materials_name_array[i] = int_m.name.c_str();
        }
    }

    out->animations_counts = static_cast<std::uint32_t>(model_info->animations.size());
    if (out->animations_counts > 0)
    {
        out->animations_array = new engine_animation_clip_create_desc_t[out->animations_counts];
        for (std::uint32_t i = 0; i < out->animations_counts; i++)
        {
            const auto& in_anim = model_info->animations[i];
            auto& anim = out->animations_array[i];
            anim.name = in_anim.name.c_str();

            std::map<std::uint32_t, engine_animation_channel_create_desc_t> channels_map;
            for (const auto& in_ch : in_anim.channels)
            {
                channels_map[in_ch.target_node_idx].model_node_index = in_ch.target_node_idx;

                engine_animation_channel_data_t* channel = nullptr;

                if (in_ch.type == engine::AnimationChannelType::eTranslation)
                {
                    channel = &channels_map[in_ch.target_node_idx].channel_translation;
                }
                else if (in_ch.type == engine::AnimationChannelType::eRotation)
                {
                    channel = &channels_map[in_ch.target_node_idx].channel_rotation;
                }
                else if (in_ch.type == engine::AnimationChannelType::eScale)
                {
                    channel = &channels_map[in_ch.target_node_idx].channel_scale;
                }
                else
                {
                    engine::log::log(engine::log::LogLevel::eError, fmt::format("Cant sucesffuly parse animation channel! Id: {}, Animation name: {}\n", i, anim.name));
                }
                if (channel)
                {
                    channel->data = in_ch.data.data();
                    channel->data_count = static_cast<std::uint32_t>(in_ch.data.size());

                    channel->timestamps = in_ch.timestamps.data();
                    channel->timestamps_count = static_cast<std::uint32_t>(in_ch.timestamps.size());
                }
            }
            anim.channels_count = channels_map.size();
            anim.channels = new engine_animation_channel_create_desc_t[anim.channels_count];
            std::size_t out_chanel_idx = 0;
            for (const auto& ch : channels_map)
            {
                anim.channels[out_chanel_idx] = ch.second;
                out_chanel_idx++;
            }
        }
    }

    out->skins_counts = static_cast<std::uint32_t>(model_info->skins.size());
    if (out->skins_counts > 0)
    {
        out->skins_array = new engine_skin_create_desc_t[out->skins_counts];

        for (std::uint32_t i = 0; i < out->skins_counts; i++)
        {
            const auto& skin = model_info->skins[i];
            auto& skin_out = out->skins_array[i];
            skin_out.name = skin.name.c_str();

            engine::log::log(engine::log::LogLevel::eTrace, fmt::format("Skin name found in model: {}\n", skin.name));
            skin_out.bones_count = static_cast<std::uint32_t>(skin.bones.size());
            if (skin_out.bones_count > 0)
            {
                skin_out.bones_array = new engine_bone_create_desc_t[skin_out.bones_count];
                for (std::uint32_t i = 0; i < skin_out.bones_count; i++)
                {
                    const auto& in_bone = skin.bones[i];
                    auto& out_bone = skin_out.bones_array[i];
                    out_bone.model_node_index = in_bone.target_node_idx;
                    std::memcpy(out_bone.inverse_bind_mat, glm::value_ptr(in_bone.inverse_bind_matrix), sizeof(in_bone.inverse_bind_matrix));
                }
            }
        }
    }

    if (!out->internal_handle)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engineApplicationAllocateModelDescAndLoadDataFromFile_2(engine_application_t handle, engine_model_specification_t spec, const char* file_name, const char* base_dir, engine_model_desc2_t* out)
{
    if (!handle || !out || !file_name || !base_dir)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    assert(*out == nullptr && "Model desc handle must be nullptr before calling this function!");
    auto* app = api_cast(handle);
    auto model_info = new engine::ModelInfo(app->load_model_desc_from_file(spec, file_name, base_dir));
    *out = reinterpret_cast<engine_model_desc2_t>(model_info);
    return ENGINE_RESULT_CODE_OK;
}

void engineApplicationReleaseModelDesc(engine_application_t handle, engine_model_desc_t* model_info)
{
    auto* app = api_cast(handle);
    app->release_model_desc(model_info);
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

void engineScenePhysicsGetCollisions(engine_scene_t scene, size_t* num_collision, const engine_collision_info_t** collisions)
{
    auto sc = api_cast(scene);
    sc->get_physcis_collisions_list(*collisions, num_collision);
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

const char* engineTexture2dDescGetName(const engine_texture_2d_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

uint32_t engineTexture2dDescGetWidth(const engine_texture_2d_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->width;
}

uint32_t engineTexture2dDescGetHeight(const engine_texture_2d_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->height;
}

engine_data_layout_t engineTexture2dDescGetDataLayout(const engine_texture_2d_desc2_t desc)
{
    return api_cast(desc)->layout;
}

const void* engineTexture2dDescGetData(const engine_texture_2d_desc2_t desc)
{
    return api_cast(desc)->data.data();
}

const char* engineGeometryDescGetName(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

const void* engineGeometryDescGetVertsData(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->vertex_data.data();
}

size_t engineGeometryDescGetVertsDataSize(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->vertex_data.size();
}

uint32_t engineGeometryDescGetVertsCount(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->vertex_count;
}

const uint32_t* engineGeometryDescGetIndsData(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->indicies.data();
}

uint32_t engineGeometryDescGetIndsCount(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->indicies.size();
}

engine_vertex_attributes_layout_t engineGeometryDescGetAttributesLayout(const engine_geometry_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->vertex_laytout;
}

const char* engineMaterialDescGetName(const engine_material_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

engine_color_desc_t engineMaterialDescGetDiffuseColor(const engine_material_desc2_t desc)
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

uint32_t engineMaterialDescGetDiffuseTextureIndex(const engine_material_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->diffuse_texture;
}

const char* engineModelNodeDescGetName(const engine_model_node_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->name.c_str();
}

engine_model_node_desc2_t engineModelNodeDescGetParent(const engine_model_node_desc2_t desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    if (typed_desc->parent)
    {
        return api_cast(typed_desc->parent.get());
    }
    return nullptr;
}

engine_model_node_desc2_t engineModelNodeDescGetChildren(const engine_model_node_desc2_t desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    if (idx < typed_desc->children.size())
    {
        return api_cast(typed_desc->children.at(idx).get());
    }
    assert("Index out of bounds for children array!");
    return nullptr;
}

uint32_t engineModelNodeDescGetChildrenCount(const engine_model_node_desc2_t desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return static_cast<uint32_t>(typed_desc->children.size());
}

uint32_t engineModelNodeDescGetIndex(const engine_model_node_desc2_t desc)
{
    return api_cast(desc)->index;
}

uint32_t engineModelNodeDescGetGeometryIndex(const engine_model_node_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->mesh;
}

uint32_t engineModelNodeDescGetSkinIndex(const engine_model_node_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->skin;
}

uint32_t engineModelNodeDescGetMaterialIndex(const engine_model_node_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->material;
}

engine_fvec3_t engineModelNodeDescGetTranslation(const engine_model_node_desc2_t desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    engine_fvec3_t ret{};
    ret.x = typed_desc->translation[0];
    ret.y = typed_desc->translation[1];
    ret.z = typed_desc->translation[2];
    return ret;
}

engine_fvec3_t engineModelNodeDescGetScale(const engine_model_node_desc2_t desc)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    engine_fvec3_t ret{};
    ret.x = typed_desc->scale[0];
    ret.y = typed_desc->scale[1];
    ret.z = typed_desc->scale[2];
    return ret;
}

engine_fvec4_t engineModelNodeDescGetRotationQuaternion(const engine_model_node_desc2_t desc)
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

const engine_model_node_desc2_t engineModelDescGetNodeDesc(const engine_model_desc2_t desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->nodes.at(idx).get());
}

uint32_t engineModelDescGetNodesDescCount(const engine_model_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->nodes.size();
}

const engine_texture_2d_desc2_t engineModelDescGetTexture2dDesc(const engine_model_desc2_t desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->textures.at(idx));
}

uint32_t engineModelDescGetTextures2dDescCount(const engine_model_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->textures.size();
}

const engine_geometry_desc2_t engineModelDescGetGeometryDesc(const engine_model_desc2_t desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->geometries.at(idx));
}

uint32_t engineModelDescGetGeometriesDescCount(const engine_model_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->geometries.size();
}

const engine_material_desc2_t engineModelDescGetMaterialDesc(const engine_model_desc2_t desc, size_t idx)
{
    assert(desc);
    const auto typed_desc = api_cast(desc);
    return api_cast(typed_desc->materials.at(idx));
}

uint32_t engineModelDescGetMaterialsDescCount(const engine_model_desc2_t desc)
{
    assert(desc);
    return api_cast(desc)->materials.size();
}
