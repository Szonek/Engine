#include "application.h"
#include "graphics.h"
#include "asset_store.h"
#include "scene.h"
#include "logger.h"
#include "gltf_parser.h"
#include "ui_document.h"
#include "scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <fmt/format.h>

#include <map>
#include <span>
#include <iostream>



namespace
{
struct vertex_attribute_simple_t
{
	std::uint32_t size;
	engine::Geometry::vertex_attribute_t::Type type;

    std::vector<float> range_min;
    std::vector<float> range_max;
};
inline std::vector<engine::Geometry::vertex_attribute_t> create_tightly_packed_vertex_layout(std::span<const vertex_attribute_simple_t> simple_attribs)
{
	using namespace engine;
	std::uint32_t stride = 0;
	std::vector<std::uint32_t> offsets;
	offsets.reserve(simple_attribs.size()); // we will use push_back();

	std::for_each(simple_attribs.begin(), simple_attribs.end(), [&stride, &offsets](const vertex_attribute_simple_t& attrib)
		{
			offsets.push_back(stride);
	std::uint32_t bytes_size = 0;
	switch (attrib.type)
	{
	case Geometry::vertex_attribute_t::Type::eFloat32:
		bytes_size = sizeof(float) * attrib.size;
        break;
    case Geometry::vertex_attribute_t::Type::eUint32:
    case Geometry::vertex_attribute_t::Type::eInt32:
        bytes_size = sizeof(std::uint32_t) * attrib.size;
        break;
    case Geometry::vertex_attribute_t::Type::eUint16:
    case Geometry::vertex_attribute_t::Type::eInt16:
        bytes_size = sizeof(std::uint16_t) * attrib.size;
        break;
    case Geometry::vertex_attribute_t::Type::eUint8:
    case Geometry::vertex_attribute_t::Type::eInt8:
        bytes_size = sizeof(std::uint8_t) * attrib.size;
        break;
    default:
        assert(false && "Unhandled case.");
	}
	stride += bytes_size;
		}
	);

	// calc total stride to the next vertex
	// construct layout
	std::vector<Geometry::vertex_attribute_t> vertex_layout;
	vertex_layout.resize(simple_attribs.size());
	for (std::uint32_t idx = 0; auto & attrib : vertex_layout)
	{
		attrib.index = idx;
		attrib.stride = stride;
		attrib.size = simple_attribs[idx].size;
		attrib.type = simple_attribs[idx].type;
		attrib.offset = offsets[idx];
        attrib.range_min = simple_attribs[idx].range_min;
        attrib.range_max = simple_attribs[idx].range_max;
		idx++;
	}
	return vertex_layout;
}
inline std::vector<engine::Geometry::vertex_attribute_t> create_engine_api_layout(const engine_vertex_attributes_layout_t& verts_layout)
{
    auto to_vert_attr_dt = [](const engine_vertex_attribute_data_type_t& dt)
    {
        switch (dt)
        {
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT32: return engine::Geometry::vertex_attribute_t::Type::eFloat32;

        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT32: return engine::Geometry::vertex_attribute_t::Type::eUint32;
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT32: return engine::Geometry::vertex_attribute_t::Type::eInt32;

        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT16: return engine::Geometry::vertex_attribute_t::Type::eUint16;
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT16: return engine::Geometry::vertex_attribute_t::Type::eInt16;

        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT8: return engine::Geometry::vertex_attribute_t::Type::eUint8;
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT8: return engine::Geometry::vertex_attribute_t::Type::eInt8;
        default:
            return engine::Geometry::vertex_attribute_t::Type::eCount;
        }
    };

    auto to_range_vector = [](const float* data, const auto size)
    {
        std::vector<float> ret(size);
        for (auto i = 0; i < ret.size(); i++)
        {
            ret.at(i) = data[i];
        }
        return ret;
    };

    std::vector<vertex_attribute_simple_t> vertex_layout_simple;
    for (auto i = 0; i < std::size(verts_layout.attributes); i++)
    {
        const auto& attr = verts_layout.attributes[i];
        if (attr.elements_count == 0)
        {
            continue;
        }
        vertex_layout_simple.push_back({ attr.elements_count, to_vert_attr_dt(attr.elements_data_type), to_range_vector(attr.range_min, attr.elements_count), to_range_vector(attr.range_max, attr.elements_count)});
    }
    return create_tightly_packed_vertex_layout(vertex_layout_simple);
}

void init_imgui(SDL_Window* wnd, SDL_GLContext gl_ctx)
{

}

}  // namespace annoymous

engine::Application::Application(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
    : rdx_(std::move(RenderContext(desc.name, { 0, 0, desc.width, desc.height }, desc.fullscreen, { init_imgui })))
    , ui_manager_(rdx_)
    , default_texture_idx_(ENGINE_INVALID_OBJECT_HANDLE)
{
	{
		//constexpr const std::array<std::uint8_t, 3> default_texture_color = { 160, 50, 168 };
		constexpr const std::array<std::uint8_t, 3> default_texture_color = { 255, 255, 255 };
		engine_texture_2d_create_desc_t tex2d_desc{};
		tex2d_desc.width = 1;
		tex2d_desc.height = 1;
        tex2d_desc.data_layout = ENGINE_DATA_LAYOUT_RGB_U8;
		tex2d_desc.data = default_texture_color.data();
        default_texture_idx_ = add_texture(tex2d_desc, "default_1x1_texutre");
	}

    {
        engine_material_create_desc_t default_material{};
        for (int i = 0; i < 4; i++)
        {
            default_material.diffuse_color[i] = 1.0f;
        }
        default_material.diffuse_texture = default_texture_idx_;
        add_material(default_material, "default_material");
    }

    rdx_.set_clear_color(0.05f, 0.0f, 0.2f, 1.0f);

	timer_.tick();

	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
}

engine::Scene* engine::Application::create_scene(const engine_scene_create_desc_t& desc)
{
    engine_result_code_t ret_code = ENGINE_RESULT_CODE_FAIL;
    auto ret = new Scene(rdx_, desc, ret_code);
    if (ret_code == ENGINE_RESULT_CODE_FAIL)
    {
        delete ret;
        return nullptr;
    }
    return ret;
}

void engine::Application::release_scene(Scene* scene)
{
    if (scene)
    {
        delete scene;
    }
}

engine_result_code_t engine::Application::update_scene(Scene* scene, float delta_time)
{
	const auto ret_code = scene->update(delta_time,
		textures_atlas_.get_objects_view(),
		geometries_atlas_.get_objects_view(),
        materials_atlas_.get_objects_view());
    return ret_code;
}

engine_application_frame_begine_info_t engine::Application::begine_frame()
{
	timer_.tick();

    engine_application_frame_begine_info_t ret{};
    ret.delta_time = static_cast<float>(timer_.delta_time().count()) / 1000.0f;
    ret.events = ENGINE_EVENT_NONE;

	for(auto& f : finger_info_buffer)
	{
		f.event_type_flags = ENGINE_FINGER_UNKNOWN;
		f.x = -1.0f;
		f.y = -1.0f;
		f.dx = 0.0f;
		f.dy = 0.0f;
	}

    /*
    if (power_save)
        has_event = SDL_WaitEventTimeout(&ev, static_cast<int>(Rml::Math::Min(context->GetNextUpdateDelay(), 10.0) * 1000));
    else
        has_event = SDL_PollEvent(&ev);
    */
    //Handle events on queue
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
        ui_manager_.parse_sdl_event(e);

        if (e.type == SDL_EVENT_QUIT)
        {
            ret.events |= ENGINE_EVENT_QUIT;
        }
        else if (e.type == SDL_EVENT_WINDOW_RESIZED)
        {
            ret.events |= ENGINE_EVENT_WINDOW_RESIZED;
            //log("Window %d resized to %dx%d", e.window.windowID, e.window.data1, e.window.data2);
        }
		else if(e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
		{
			viewport_t vp{};
			vp.width = e.window.data1;
			vp.height = e.window.data2;
			rdx_.set_viewport(vp);
		}
        else if (e.type == SDL_EVENT_WINDOW_MOVED)
        {
            ret.events |= ENGINE_EVENT_WINDOW_MOVED;
        }
		else if(e.type == SDL_EVENT_FINGER_UP)
		{
			//const auto str = fmt::format("[SDL_EVENT_FINGER_UP]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			//log::log(log::LogLevel::eTrace, str.c_str());
			//auto& f = finger_info_buffer[e.tfinger.fingerId];
			//f.event_type_flags |= ENGINE_FINGER_UP;
			//f.x = e.tfinger.x;
			//f.y = e.tfinger.y;
		}
		else if(e.type == SDL_EVENT_FINGER_DOWN)
		{
			//const auto str = fmt::format("[SDL_EVENT_FINGER_DOWN]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			//log::log(log::LogLevel::eTrace, str.c_str());
			//auto& f = finger_info_buffer[e.tfinger.fingerId];
			//f.event_type_flags |= ENGINE_FINGER_DOWN;
			//f.x = e.tfinger.x;
			//f.y = e.tfinger.y;
		}
		else if(e.type == SDL_EVENT_FINGER_MOTION)
		{
			//auto& f = finger_info_buffer[e.tfinger.fingerId];
			//f.event_type_flags |= ENGINE_FINGER_MOTION;
			//f.x = e.tfinger.x;
			//f.y = e.tfinger.y;
			//f.dx += e.tfinger.dx;
			//f.dy += e.tfinger.dy;
            //
			//const auto str = fmt::format("[SDL_EVENT_FINGER_MOTION]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			//log::log(log::LogLevel::eTrace, str.c_str());
		}
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            // ?
        }
        else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            // ?
        }
    }

    // flip the coords so it matches mouse and expected coords are: (0, 0) left bottom corner;  (1,1) top right corner
    for(auto& f : finger_info_buffer)
    {
        f.y = 1.0f - f.y;
        f.dy = -1.0f * f.dy;
    }

	rdx_.begin_frame();
	return ret;
}

engine_application_frame_end_info_t engine::Application::end_frame()
{
    ui_manager_.update_state_and_render();
    rdx_.end_frame();
	engine_application_frame_end_info_t ret{};
	//ret.success = !glfwWindowShouldClose(rdx_.get_glfw_window());;
    ret.success = true;
	return ret;
}

std::uint32_t engine::Application::add_texture(const engine_texture_2d_create_desc_t& desc, std::string_view texture_name)
{
    const auto data_layout = [](const auto engine_api_layout)
    {
        switch (engine_api_layout)
        {
        case ENGINE_DATA_LAYOUT_RGBA_FP32: return DataLayout::eRGBA_FP32;
        case ENGINE_DATA_LAYOUT_R_FP32: return DataLayout::eR_FP32;

        case ENGINE_DATA_LAYOUT_RGBA_U8: return DataLayout::eRGBA_U8;
        case ENGINE_DATA_LAYOUT_RGB_U8: return DataLayout::eRGB_U8;
        case ENGINE_DATA_LAYOUT_R_U8: return DataLayout::eR_U8;
        default:
            return DataLayout::eCount;
        }
    }(desc.data_layout);
	return textures_atlas_.add_object(texture_name, Texture2D(desc.width, desc.height, true, desc.data, data_layout, TextureAddressClampMode::eClampToEdge));
}

std::uint32_t engine::Application::add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t /*color_space*/)
{
	return textures_atlas_.add_object(texture_name, Texture2D(file_name, true));
}

std::uint32_t engine::Application::get_texture(std::string_view name) const
{
    const auto ret = textures_atlas_.get_object(name);
    return ret;
}

std::uint32_t engine::Application::add_font_from_file(std::string_view file_name, std::string_view handle_name)
{
    const auto res = ui_manager_.load_font_from_file(file_name, handle_name);
    assert(res != ENGINE_INVALID_OBJECT_HANDLE && "Failed loading font from file!");
    return res;
}

std::uint32_t engine::Application::get_font(std::string_view name) const
{
    return ui_manager_.get_font(name);
}

std::uint32_t engine::Application::add_geometry(const engine_vertex_attributes_layout_t& api_verts_layout, std::int32_t vertex_count, std::span<const std::byte> verts_data, std::span<const uint32_t> inds, std::string_view name)
{
	const auto vertex_layout = create_engine_api_layout(api_verts_layout);
	return geometries_atlas_.add_object(name, std::move(Geometry(vertex_layout, verts_data, vertex_count, inds)));
}

std::uint32_t engine::Application::get_geometry(std::string_view name) const
{
    return geometries_atlas_.get_object(name);
}

const engine::Geometry* engine::Application::get_geometry(std::uint32_t idx) const
{
    return geometries_atlas_.get_object(idx);
}

std::uint32_t engine::Application::add_material(const engine_material_create_desc_t& desc, std::string_view name)
{
    return materials_atlas_.add_object(name, engine_material_create_desc_t(desc));
}

std::uint32_t engine::Application::get_material(std::string_view name) const
{
    return materials_atlas_.get_object(name);
}

engine_model_desc_t engine::Application::load_model_desc_from_file(engine_model_specification_t spec, std::string_view name)
{
    assert(spec == ENGINE_MODEL_SPECIFICATION_GLTF_2);

    const auto file_data = engine::AssetStore::get_instance().get_model_data(name);
    if(file_data.get_size() == 0)
    {
        return {};
    }
    const auto model_info = new engine::ModelInfo(parse_gltf_data_from_memory({ file_data.get_data_ptr(), file_data.get_size() }));

    engine_model_desc_t ret{};
    ret.internal_handle = reinterpret_cast<const void*>(model_info);
    
    ret.nodes_count = static_cast<std::uint32_t>(model_info->nodes.size());
    if (ret.nodes_count > 0)
    {
        ret.nodes_array = new engine_model_node_desc_t[ret.nodes_count];
        for (std::size_t i = 0; i < ret.nodes_count; i++)
        {
            auto copy_arr = [](float* const arr, const auto& glm_vec)
            {
                for (auto i = 0; i < glm_vec.length(); i++)
                {
                    arr[i] = glm_vec[i];
                }
            };

            const auto& in_n = model_info->nodes.at(i);
            auto& ret_n = ret.nodes_array[i];
            ret_n.geometry_index = in_n->mesh;
            ret_n.skin_index = in_n->skin;
            ret_n.name = in_n->name.c_str();
            if (ret_n.geometry_index != -1)
            {
                ret_n.material_index = model_info->geometries[ret_n.geometry_index].material_index;
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
                ret_n.parent = &ret.nodes_array[in_n->parent->index];
            }
            else
            {
                ret_n.parent = nullptr;
            }
        }
    }

    ret.geometries_count = static_cast<std::uint32_t>(model_info->geometries.size());
    if (ret.geometries_count > 0)
    {
        ret.geometries_array = new engine_geometry_create_desc_t[ret.geometries_count];

        for (std::size_t i = 0; i < ret.geometries_count; i++)
        {
            const auto& int_g = model_info->geometries[i];
            auto& ret_g = ret.geometries_array[i];

            ret_g.inds_count = int_g.indicies.size();
            ret_g.inds = int_g.indicies.data();

            ret_g.verts_data_size = int_g.vertex_data.size();
            ret_g.verts_data = int_g.vertex_data.data();
            ret_g.verts_layout = int_g.vertex_laytout;
            ret_g.verts_count = int_g.vertex_count;
        }

    }

    ret.textures_count = static_cast<std::uint32_t>(model_info->textures.size());
    if (ret.textures_count > 0)
    {
        ret.textures_array = new engine_texture_2d_create_desc_t[ret.textures_count];
        for (std::size_t i = 0; i < ret.textures_count; i++)
        {
            const auto& int_m = model_info->textures[i];
            auto& ret_m = ret.textures_array[i];

            ret_m.width = int_m.width;
            ret_m.height = int_m.height;
            ret_m.data_layout = int_m.layout;
            ret_m.data = int_m.data.data();
        }
    }

    ret.materials_count = static_cast<std::uint32_t>(model_info->materials.size());
    if (ret.materials_count > 0)
    {
        ret.materials_array = new engine_model_material_desc_t[ret.materials_count];

        for (std::size_t i = 0; i < ret.materials_count; i++)
        {
            const auto& int_m = model_info->materials[i];
            auto& ret_m = ret.materials_array[i];

            ret_m.name = int_m.name.c_str();
            std::memcpy(ret_m.diffuse_color, int_m.diffuse_factor.data(), int_m.diffuse_factor.size() * sizeof(int_m.diffuse_factor[0]));
            ret_m.diffuse_texture_index = int_m.diffuse_texture;
        }
    }

    ret.animations_counts = static_cast<std::uint32_t>(model_info->animations.size());
    if (ret.animations_counts > 0)
    {
        ret.animations_array = new engine_animation_clip_create_desc_t[ret.animations_counts];
        for (std::uint32_t i = 0; i < ret.animations_counts; i++)
        {
            const auto& in_anim = model_info->animations[i];
            auto& anim = ret.animations_array[i];
            anim.name = in_anim.name.c_str();

            std::map<std::uint32_t, engine_animation_channel_create_desc_t> channels_map;
            for (const auto& in_ch : in_anim.channels)
            {
                channels_map[in_ch.target_node_idx].model_node_index = in_ch.target_node_idx;

                engine_animation_channel_data_t* channel = nullptr;

                if (in_ch.type == AnimationChannelType::eTranslation)
                {
                    channel = &channels_map[in_ch.target_node_idx].channel_translation;
                }
                else if (in_ch.type == AnimationChannelType::eRotation)
                {
                    channel = &channels_map[in_ch.target_node_idx].channel_rotation;
                }
                else if (in_ch.type == AnimationChannelType::eScale)
                {
                    channel = &channels_map[in_ch.target_node_idx].channel_scale;
                }
                else
                {
                    log::log(log::LogLevel::eError, fmt::format("Cant sucesffuly parse animation channel! Id: {}, Animation name: {}\n", i, anim.name));
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
    
    ret.skins_counts = static_cast<std::uint32_t>(model_info->skins.size());
    if (ret.skins_counts > 0)
    {
        ret.skins_array = new engine_skin_create_desc_t[ret.skins_counts];
        for (std::uint32_t i = 0; i < ret.skins_counts; i++)
        {
            const auto& skin = model_info->skins[i];
            auto& skin_out = ret.skins_array[i];
            skin_out.name = skin.name.c_str();

            log::log(log::LogLevel::eTrace, fmt::format("Skin name found in model: {}\n", skin.name));
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

    return ret;
}

void engine::Application::release_model_desc(engine_model_desc_t* info)
{
    if (info)
    {
        const auto model_info = reinterpret_cast<const engine::ModelInfo*>(info->internal_handle);
        delete model_info;
        if (info->geometries_array)
        {
            delete[] info->geometries_array;
        }
        if (info->textures_array)
        {
            delete[] info->textures_array;
        }
        if (info->materials_array)
        {
            delete[] info->materials_array;
        }
        if (info->animations_array)
        {
            for (std::uint32_t i = 0; i < info->animations_counts; i++)
            {
                delete[] info->animations_array[i].channels;
            }
            delete[] info->animations_array;
        }
        if (info->skins_array)
        {
            for (std::uint32_t i = 0; i < info->skins_counts; i++)
            {
                if (info->skins_array[i].bones_count > 0)
                {
                    delete[] info->skins_array[i].bones_array;
                }
            }
            delete[] info->skins_array;
        }
        std::memset(info, 0, sizeof(engine_model_desc_t));
    }
}

engine::UiDocument engine::Application::load_ui_document(std::string_view file_name)
{
    return ui_manager_.load_document_from_file(file_name);
}


engine::UiDataHandle engine::Application::create_ui_document_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings)
{
    return ui_manager_.create_data_handle(name, bindings);
}

bool engine::Application::keyboard_is_key_down(engine_keyboard_keys_t key)
{
    const auto state = SDL_GetKeyboardState(nullptr);
    return static_cast<bool>(state[key]);
}

engine_coords_2d_t engine::Application::mouse_get_coords()
{
	float coord_x = 0.;
	float coord_y = 0.;
    SDL_GetMouseState(&coord_x, &coord_y);

    const auto window_size = rdx_.get_window_size_in_pixels();

    engine_coords_2d_t ret{};
	ret.x = static_cast<std::int32_t>(std::floor(coord_x)) / static_cast<float>(window_size.width);
    // flip Y coords so left, bottom corner is (0, 0) and right top is (1, 1)
    ret.y = 1.0f - static_cast<std::int32_t>(std::floor(coord_y)) / static_cast<float>(window_size.height);
	return ret;
}

bool engine::Application::mouse_is_button_down(engine_mouse_button_t button)
{
    const auto state = SDL_GetMouseState(nullptr, nullptr);
    return state & SDL_BUTTON(button);
}

std::array<engine_finger_info_t, 10> engine::Application::get_finger_info_events() const
{
	return finger_info_buffer;
}