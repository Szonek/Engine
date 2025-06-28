#include "application.h"
#include "graphics.h"
#include "asset_store.h"
#include "scene.h"
#include "logger.h"
#include "gltf_parser.h"
#include "ui_document.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <fmt/format.h>

#include <map>
#include <span>
#include <iostream>

#include "profiler.h"

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

}  // namespace annoymous

engine::Application::Application(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
    : rdx_(std::move(RenderContext(desc.name, { 0, 0, desc.width, desc.height }, desc.fullscreen)))
    , ui_manager_(rdx_)
    , default_texture_idx_(ENGINE_INVALID_OBJECT_HANDLE)
    , fbo_scene_(rdx_.get_window_size_in_pixels().width, rdx_.get_window_size_in_pixels().height, { DataLayout::eRGBA_U8, DataLayout::eR_U32 }, true)
    , empty_vao_for_full_screen_quad_draw_(6)
    , shader_full_screen_quad_(Shader({ "full_screen_quad.vs" }, { "full_screen_quad.fs" }))
{
	{
		const std::vector<std::byte> default_texture_color = { static_cast<std::byte>(255), static_cast<std::byte>(255), static_cast<std::byte>(255) };
		TextureInfo tex2d_desc{};
        tex2d_desc.name = "default_texture";
		tex2d_desc.width = 1;
		tex2d_desc.height = 1;
        tex2d_desc.layout = ENGINE_DATA_LAYOUT_RGB_U8;
        tex2d_desc.data = default_texture_color;
        default_texture_idx_ = add_texture(tex2d_desc);
	}

	timer_.tick();

	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
    if (default_texture_idx_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        destroy_texture(default_texture_idx_);
    }
}

engine::Scene* engine::Application::allocate_scene(const engine_scene_create_desc_t& desc)
{
    engine_result_code_t ret_code = ENGINE_RESULT_CODE_FAIL;
    auto ret = new Scene(this, rdx_, desc, ret_code);
    if (ret_code == ENGINE_RESULT_CODE_FAIL)
    {
        delete ret;
        return nullptr;
    }
    const auto invalid_entity_id = ret->create_new_entity();
    assert(ENGINE_INVALID_OBJECT_HANDLE != static_cast<std::uint32_t>(invalid_entity_id)); // add invalid game object id
    on_scene_create(ret);
    return ret;
}

void engine::Application::release_scene(Scene* scene)
{
    if (scene)
    {
        on_scene_release(scene);
        delete scene;
    }
}

engine_result_code_t engine::Application::update_scene(Scene* scene, float delta_time)
{
    on_scene_update_pre(scene, delta_time);
	const auto ret_code = scene->update(delta_time);
    on_scene_update_post(scene, delta_time);

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
        on_sdl_event(e);
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
    
    // clear framebuffer at beginning of the frame, scene (camera) will have to call set_viewport(..)!
    const auto& [win_w, win_h] = rdx_.get_window_size_in_pixels();
    {
        fbo_scene_.bind();
        const auto& [fbo_w, fbo_h] = fbo_scene_.get_size();
        if (fbo_w != win_w || fbo_h != win_h)
        {
            fbo_scene_.resize(win_w, win_h);
        }
        rdx_.set_clear_color(0.00f, 0.0f, 0.0f, 1.0f);
        fbo_scene_.clear();
    }
    
    on_frame_begine(ret);
	return ret;
}

engine_application_frame_end_info_t engine::Application::end_frame()
{
    on_frame_end();
    // copy fbo_scene color attachment to the default framebuffer
    fbo_scene_.unbind();
    shader_full_screen_quad_.bind();
    shader_full_screen_quad_.set_texture_with_sampler("screen_texture", fbo_scene_.get_color_attachment(0));
    empty_vao_for_full_screen_quad_draw_.bind();
    empty_vao_for_full_screen_quad_draw_.draw(Geometry::Mode::eTriangles);

    ui_manager_.update_state_and_render();
    rdx_.end_frame();
    ENGINE_PROFILE_FRAME;
	engine_application_frame_end_info_t ret{};
	//ret.success = !glfwWindowShouldClose(rdx_.get_glfw_window());;
    ret.success = true;
	return ret;
}

std::uint32_t engine::Application::add_texture(const TextureInfo& desc)
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
        }(desc.layout);
    return textures_atlas_.add_object(desc.name, Texture2D(desc.width, desc.height, true, desc.data.data(), data_layout, TextureAddressClampMode::eClampToEdge));
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

const engine::Texture2D* engine::Application::get_texture(std::uint32_t idx) const
{
    return textures_atlas_.get_object(idx);
}

void engine::Application::destroy_texture(std::uint32_t idx)
{
    textures_atlas_.remove_object(idx);
}

std::uint32_t engine::Application::add_nav_mesh(std::string_view name)
{
    return nav_mesh_atlas_.add_object(name, NavMesh());
}

std::uint32_t engine::Application::get_nav_mesh(std::string_view name) const
{
    return nav_mesh_atlas_.get_object(name);
}

const engine::NavMesh* engine::Application::get_nav_mesh(std::uint32_t idx) const
{
    return nav_mesh_atlas_.get_object(idx);
}

void engine::Application::destroy_nav_mesh(std::uint32_t idx)
{
    nav_mesh_atlas_.remove_object(idx);
}

bool engine::Application::add_font_from_file(std::string_view file_name, std::string_view handle_name)
{
    const auto res = ui_manager_.load_font_from_file(file_name, handle_name);
    return res;
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

std::string engine::Application::get_geometry_name(std::uint32_t idx) const
{
    return geometries_atlas_.get_object_name(idx);
}

const engine::Geometry* engine::Application::get_geometry(std::uint32_t idx) const
{
    return geometries_atlas_.get_object(idx);
}

void engine::Application::destroy_geometry(std::uint32_t idx)
{
    geometries_atlas_.remove_object(idx);
}

std::uint32_t engine::Application::add_shader(const std::vector<std::string>& vertex_shader_name, const std::vector<std::string>& fragment_shader_name, std::string_view name)
{
    return shader_atlas_.add_object(name, Shader(vertex_shader_name, fragment_shader_name));
}

std::uint32_t engine::Application::get_shader(std::string_view name) const
{
    return shader_atlas_.get_object(name);
}

void engine::Application::destroy_shader(std::uint32_t idx)
{
    shader_atlas_.remove_object(idx);
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

engine_fvec2_t engine::Application::mouse_get_coords()
{
	float coord_x = 0.;
	float coord_y = 0.;
    SDL_GetMouseState(&coord_x, &coord_y);

    const auto window_size = rdx_.get_window_size_in_pixels();

    engine_fvec2_t ret{};
	ret.x = static_cast<std::int32_t>(std::floor(coord_x)) / static_cast<float>(window_size.width);
    // flip Y coords so left, bottom corner is (0, 0) and right top is (1, 1)
    ret.y = 1.0f - static_cast<std::int32_t>(std::floor(coord_y)) / static_cast<float>(window_size.height);
	return ret;
}

bool engine::Application::mouse_is_button_down(engine_mouse_button_t button)
{
    const auto state = SDL_GetMouseState(nullptr, nullptr);
    return state & SDL_BUTTON_MASK(button);
}

std::array<engine_finger_info_t, 10> engine::Application::get_finger_info_events() const
{
	return finger_info_buffer;
}