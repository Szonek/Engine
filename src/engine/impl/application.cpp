#include "application.h"
#include "graphics.h"
#include "asset_store.h"
#include "scene.h"

#include <GLFW/glfw3.h>

#include <span>
#include <iostream>
#include <ranges>

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

	std::ranges::for_each(simple_attribs, [&stride, &offsets](const vertex_attribute_simple_t& attrib)
		{
			offsets.push_back(stride);
	std::uint32_t bytes_size = 0;
	switch (attrib.type)
	{
	case Geometry::vertex_attribute_t::Type::eFloat:
		bytes_size = sizeof(float) * attrib.size;
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
	: rdx_(std::move(RenderContext(desc.name, {0, 0, desc.width, desc.height})))
{
	if (desc.asset_store_path)
	{
		//ToDo: make this per application. Multiple application would overwrite this singletons configurables.
		engine::AssetStore::get_instance().configure_base_path(desc.asset_store_path);
	}

	{
		constexpr const std::array<std::uint8_t, 3> default_texture_color = { 160, 50, 168 };
		engine_texture_2d_create_from_memory_desc_t desc{};
		desc.width = 1;
		desc.height = 1;
		desc.channels = 3;
		desc.color_space = ENGINE_TEXTURE_COLOR_SPACE_LINEAR;
		desc.data = default_texture_color.data();
		add_texture_from_memory(desc, "default_1x1_texutre");
	}


	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
	glfwTerminate();
}

engine_result_code_t engine::Application::run_scene(Scene* scene, float delta_time)
{
	return scene->update(rdx_, delta_time,
		textures_atlas_.get_objects_view(),
		geometries_atlas_.get_objects_view());
}

engine_application_frame_begine_info_t engine::Application::begine_frame()
{
	timer_.tick();
	glfwPollEvents();

	rdx_.begin_frame();

	engine_application_frame_begine_info_t ret{};
	ret.delta_time = timer_.delta_time();
	return ret;
}

engine_application_frame_end_info_t engine::Application::end_frame()
{
	rdx_.end_frame();
	engine_application_frame_end_info_t ret{};
	ret.success = !glfwWindowShouldClose(rdx_.get_glfw_window());;
	return ret;
}

std::uint32_t engine::Application::add_texture_from_memory(const engine_texture_2d_create_from_memory_desc_t& desc, std::string_view texture_name)
{
	return textures_atlas_.add_object(texture_name, Texture2D(desc.width, desc.height, desc.channels, true, desc.data));
}

std::uint32_t engine::Application::add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t color_space)
{
	return textures_atlas_.add_object(texture_name, Texture2D(file_name, true));
}

std::uint32_t engine::Application::add_geometry_from_memory(std::span<const engine_vertex_attribute_t> verts, std::span<const uint32_t> inds, std::string_view name)
{
	const static auto vertex_layout = create_engine_api_layout();
	return geometries_atlas_.add_object(name, Geometry(vertex_layout, { reinterpret_cast<const std::byte*>(verts.data()), verts.size_bytes() }, verts.size(), inds));
}

bool engine::Application::keyboard_is_key_down(engine_keyboard_keys_t key) const
{
	return false;
}

engine_mouse_coords_t engine::Application::mouse_get_coords() const
{
	return engine_mouse_coords_t();
}

bool engine::Application::mouse_is_button_down(engine_mouse_button_t button) const
{
	return false;
}
