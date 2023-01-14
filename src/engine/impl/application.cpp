#include "application.h"
#include "graphics.h"
#include "asset_store.h"
#include "scene.h"

#include <GLFW/glfw3.h>

#include <span>
#include <iostream>
#include <ranges>

engine::Application::Application(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
	: rdx_(std::move(RenderContext(desc.name, {0, 0, desc.width, desc.height})))
{
	if (desc.asset_store_path)
	{
		//ToDo: make this per application. Multiple application would overwrite this singletons configurables.
		engine::AssetStore::get_instance().configure_base_path(desc.asset_store_path);
	}
	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
	glfwTerminate();
}

engine_result_code_t engine::Application::run_scene(Scene* scene, float delta_time)
{
	return scene->update(rdx_, delta_time);
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
