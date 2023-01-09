#include "application.h"
#include "graphics.h"
#include "asset_store.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

inline auto create_geometry()
{
	using namespace engine;
	// create geometry
	std::vector<Geometry::vertex_attribute_t> vertex_layout;
	vertex_layout.reserve(10);
	vertex_layout.push_back({ Geometry::vertex_attribute_t{0, 3, 8 * sizeof(float), 0, Geometry::vertex_attribute_t::Type::eFloat} });
	vertex_layout.push_back({ Geometry::vertex_attribute_t{1, 3, 8 * sizeof(float), 3 * sizeof(float), Geometry::vertex_attribute_t::Type::eFloat} });
	vertex_layout.push_back({ Geometry::vertex_attribute_t{2, 2, 8 * sizeof(float), 6 * sizeof(float), Geometry::vertex_attribute_t::Type::eFloat} });
	float vertices[] = {
		// positions          // colors           // texture coords
		 0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
		 0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
		-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
		-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // top left 
	};
	std::uint32_t indices[] =
	{
		0, 1, 3,   // first triangle
		1, 2, 3    // second triangle
	};
	return Geometry(vertex_layout, vertices, indices);
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
	// transform system
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0, 0.0, 1.0));
		model = glm::scale(model, glm::vec3(0.5, 0.5, 0.5));
		shader_.set_uniform_mat4f("model_matrix", { glm::value_ptr(model), sizeof(model) / sizeof(float) });
	}

	// render system
	{	
		texture_container_.bind(0);
		texture_face_.bind(1);

		geometry_.bind();
		geometry_.draw(Geometry::Mode::eTriangles);
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
