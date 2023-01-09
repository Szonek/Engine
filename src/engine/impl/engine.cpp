#include <engine.h>

#include "application.h"
#include "asset_store.h"

engine_result_code_t engineApplicationCreate(engine_application_t* handle, engine_application_create_desc_t create_desc)
{
	if (create_desc.asset_store_path)
	{
		//ToDo: make this per application. Multiple application would overwrite this singletons configurables.
		engine::AssetStore::get_instance().configure_base_path(create_desc.asset_store_path);
	}

	engine_result_code_t ret = ENGINE_RESULT_CODE_FAIL;
	*handle = reinterpret_cast<engine_application_t>(new engine::Application(create_desc, ret));

	return ret;
}

void engineApplicationDestroy(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	delete app;
}

bool engineApplicationIsKeyboardButtonDown(engine_application_t handle, engine_keyboard_keys_t key)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->keyboard_is_key_down(key);
}

bool engineApplicationIsKeyboardButtonUp(engine_application_t handle, engine_keyboard_keys_t key)
{
	return !engineApplicationIsKeyboardButtonDown(handle, key);
}

engine_mouse_coords_t engineApplicationGetMouseCoords(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->mouse_get_coords();
}

bool engineApplicationIsMouseButtonDown(engine_application_t handle, engine_mouse_button_t button)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->mouse_is_button_down(button);
}

bool engineApplicationIsMouseButtonUp(engine_application_t handle, engine_mouse_button_t button)
{
	return !engineApplicationIsMouseButtonDown(handle, button);
}

engine_application_frame_begine_info_t engineApplicationFrameBegine(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->begine_frame();
}

engine_result_code_t engineApplicationFrameRunScene(engine_application_t handle, engine_scene_t scene, float delta_time)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	auto* scene_typed = reinterpret_cast<engine::Scene*>(scene);
	return app->run_scene(scene_typed, delta_time);
}

engine_application_frame_end_info_t engineApplicationFrameEnd(engine_application_t handle)
{
	auto* app = reinterpret_cast<engine::Application*>(handle);
	return app->end_frame();
}