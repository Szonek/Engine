#include "application.h"
#include "graphics.h"
#include "asset_store.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <span>
#include <iostream>
#include <ranges>

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

inline auto create_geometry()
{
	using namespace engine;
	std::vector< vertex_attribute_simple_t> vertex_layout_simple;
	vertex_layout_simple.push_back({ 3,  Geometry::vertex_attribute_t::Type::eFloat });
	vertex_layout_simple.push_back({ 2,  Geometry::vertex_attribute_t::Type::eFloat });
	float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};
	//std::uint32_t indices[] =
	//{
	//	0, 1, 3,   // first triangle
	//	1, 2, 3    // second triangle
	//};
	return Geometry(create_tightly_packed_vertex_layout(vertex_layout_simple), vertices, {});
}

engine::Application::Application(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
	: rdx_(std::move(RenderContext(desc.name, {0, 0, desc.width, desc.height})))
	, shader_(Shader("simple.vs", "simple.fs"))
	, geometry_(create_geometry())
	, texture_container_(Texture2D("container.jpg", true))
	, texture_face_(Texture2D("awesomeface.png", true))
{
	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
	glfwTerminate();
}

engine_result_code_t engine::Application::run_scene(Scene* scene, float delta_time)
{
	shader_.bind();

	const std::vector<glm::vec3> cubes_positions =
	{
		glm::vec3(0.0f,  0.0f,  0.0f),
		glm::vec3(2.0f,  5.0f, -15.0f),
		glm::vec3(-1.5f, -2.2f, -2.5f),
		glm::vec3(-3.8f, -2.0f, -12.3f),
		glm::vec3(2.4f, -0.4f, -3.5f),
		glm::vec3(-1.7f,  3.0f, -7.5f),
		glm::vec3(1.3f, -2.0f, -2.5f),
		glm::vec3(1.5f,  2.0f, -2.5f),
		glm::vec3(1.5f,  0.2f, -1.5f),
		glm::vec3(-1.3f,  1.0f, -1.5f)
	};

	// camera system
	{
		glm::mat4 view = glm::mat4(1.0f);
		// note that we're translating the scene in the reverse direction of where we want to move
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

		std::int32_t width = -1;
		std::int32_t height = -1;
		glfwGetWindowSize(rdx_.get_glfw_window(), &width, &height);
		glm::mat4 projection;
		const auto aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
		projection = glm::perspective(glm::radians(45.0f), aspect_ratio, 0.1f, 100.0f);

		shader_.set_uniform_mat4f("view", { glm::value_ptr(view), sizeof(view) / sizeof(float) });
		shader_.set_uniform_mat4f("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });
	}


	// transform system
	{
		for (auto i = 0; i < cubes_positions.size(); i++)
		{
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, cubes_positions[i]);
			float angle = 20.0f * i;
			model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
			shader_.set_uniform_mat4f("model", { glm::value_ptr(model), sizeof(model) / sizeof(float) });

			texture_container_.bind(0);
			texture_face_.bind(1);

			geometry_.bind();
			geometry_.draw(Geometry::Mode::eTriangles);
		}
		//glm::mat4 model = glm::mat4(1.0f);
		//model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));
		//shader_.set_uniform_mat4f("model", { glm::value_ptr(model), sizeof(model) / sizeof(float) });
	}

	// render system
	{	
		//texture_container_.bind(0);
		//texture_face_.bind(1);

		//geometry_.bind();
		//geometry_.draw(Geometry::Mode::eTriangles);
	}


	return ENGINE_RESULT_CODE_OK;
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
