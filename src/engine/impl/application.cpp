#include "application.h"
#include "graphics.h"
#include "asset_store.h"
#include "scene.h"
#include "logger.h"
#include "gltf_parser.h"

#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <fmt/format.h>

#include <span>
#include <iostream>

namespace
{
struct vertex_attribute_simple_t
{
	std::uint32_t size;
	engine::Geometry::vertex_attribute_t::Type type;
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
	case Geometry::vertex_attribute_t::Type::eFloat:
		bytes_size = sizeof(float) * attrib.size;
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
		idx++;
	}
	return vertex_layout;
}
inline std::vector<engine::Geometry::vertex_attribute_t> create_engine_api_layout()
{
	std::vector< vertex_attribute_simple_t> vertex_layout_simple;
	vertex_layout_simple.push_back({ 3,  engine::Geometry::vertex_attribute_t::Type::eFloat });
	vertex_layout_simple.push_back({ 2,  engine::Geometry::vertex_attribute_t::Type::eFloat });
	return create_tightly_packed_vertex_layout(vertex_layout_simple);
}

}  // namespace annoymous

engine::Application::Application(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
	: rdx_(std::move(RenderContext(desc.name, {0, 0, desc.width, desc.height}, desc.fullscreen)))
{
	{
		constexpr const std::array<std::uint8_t, 3> default_texture_color = { 160, 50, 168 };
		engine_texture_2d_create_from_memory_desc_t desc{};
		desc.width = 1;
		desc.height = 1;
        desc.data_layout = ENGINE_DATA_LAYOUT_RGB_U8;
		desc.data = default_texture_color.data();
		add_texture_from_memory(desc, "default_1x1_texutre");
	}

    rdx_.set_clear_color(0.05f, 0.0f, 0.2f, 1.0f);

	timer_.tick();
	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
	//glfwTerminate();
}

engine_result_code_t engine::Application::update_scene(Scene* scene, float delta_time)
{
	const auto ret_code = scene->update(rdx_, delta_time,
		textures_atlas_.get_objects_view(),
		geometries_atlas_.get_objects_view(),
        &ui_manager_);
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

    //Handle events on queue
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
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
			const auto str = fmt::format("[SDL_EVENT_FINGER_UP]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			log::log(log::LogLevel::eTrace, str.c_str());
			auto& f = finger_info_buffer[e.tfinger.fingerId];
			f.event_type_flags |= ENGINE_FINGER_UP;
			f.x = e.tfinger.x;
			f.y = e.tfinger.y;
		}
		else if(e.type == SDL_EVENT_FINGER_DOWN)
		{
			const auto str = fmt::format("[SDL_EVENT_FINGER_DOWN]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			log::log(log::LogLevel::eTrace, str.c_str());
			auto& f = finger_info_buffer[e.tfinger.fingerId];
			f.event_type_flags |= ENGINE_FINGER_DOWN;
			f.x = e.tfinger.x;
			f.y = e.tfinger.y;
		}
		else if(e.type == SDL_EVENT_FINGER_MOTION)
		{
			auto& f = finger_info_buffer[e.tfinger.fingerId];
			f.event_type_flags |= ENGINE_FINGER_MOTION;
			f.x = e.tfinger.x;
			f.y = e.tfinger.y;
			f.dx += e.tfinger.dx;
			f.dy += e.tfinger.dy;

			const auto str = fmt::format("[SDL_EVENT_FINGER_MOTION]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			log::log(log::LogLevel::eTrace, str.c_str());
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

	rdx_.begin_frame();
    const auto window_size_pixels = rdx_.get_window_size_in_pixels();
    ui_manager_.begin_frame(static_cast<float>(window_size_pixels.width), static_cast<float>(window_size_pixels.height));
	return ret;
}

engine_application_frame_end_info_t engine::Application::end_frame()
{
    ui_manager_.end_frame();
    rdx_.end_frame();
	engine_application_frame_end_info_t ret{};
	//ret.success = !glfwWindowShouldClose(rdx_.get_glfw_window());;
    ret.success = true;
	return ret;
}

std::uint32_t engine::Application::add_texture_from_memory(const engine_texture_2d_create_from_memory_desc_t& desc, std::string_view texture_name)
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

std::uint32_t engine::Application::add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t color_space)
{
	return textures_atlas_.add_object(texture_name, Texture2D(file_name, true));
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

std::uint32_t engine::Application::add_geometry_from_memory(std::span<const engine_vertex_attribute_t> verts, std::span<const uint32_t> inds, std::string_view name)
{
	const static auto vertex_layout = create_engine_api_layout();
	return geometries_atlas_.add_object(name, std::move(Geometry(vertex_layout, { reinterpret_cast<const std::byte*>(verts.data()), verts.size_bytes() }, verts.size(), inds)));
}

std::uint32_t engine::Application::get_geometry(std::string_view name) const
{
    return geometries_atlas_.get_object(name);
;}


engine_model_info_t engine::Application::load_model_info_from_file(engine_model_specification_t spec, std::string_view name)
{
    const auto file_data = engine::AssetStore::get_instance().get_model_data(name);
    if(file_data.get_size() == 0)
    {
        return {};
    }
    const auto model_info = new engine::ModelInfo(parse_gltf_data_from_memory({ file_data.get_data_ptr(), file_data.get_size() }));

    engine_model_info_t ret{};
    ret.internal_handle = reinterpret_cast<const void*>(model_info);
    ret.geometries_count = model_info->geometries.size();
    ret.geometries_array = new engine_geometry_info_t[ret.geometries_count];

    for (std::size_t i = 0; i < ret.geometries_count; i++)
    {
        const auto& int_g = model_info->geometries[i];
        auto& ret_g = ret.geometries_array[i];

        ret_g.inds_count = int_g.indicies.size();
        ret_g.inds = int_g.indicies.data();

        ret_g.verts_count = int_g.verticies.size();
        ret_g.verts = int_g.verticies.data();
    }

    return ret;
}

void engine::Application::release_model_info(engine_model_info_t* info)
{
    if (info)
    {
        const auto model_info = reinterpret_cast<const engine::ModelInfo*>(info->internal_handle);
        delete model_info;
        std::memset(info, 0, sizeof(engine_model_info_t));
    }
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

std::span<const engine_finger_info_t> engine::Application::get_finger_info_events() const
{
	return finger_info_buffer;
}