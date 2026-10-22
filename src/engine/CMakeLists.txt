set(ENGINE "engine")

set(ENGINE_SOURCES_DIR "impl")
set(ENGINE_SOURCES
	${ENGINE_SOURCES_DIR}/graphics.cpp
	${ENGINE_SOURCES_DIR}/graphics.h
	${ENGINE_SOURCES_DIR}/scene.cpp
	${ENGINE_SOURCES_DIR}/scene.h
	${ENGINE_SOURCES_DIR}/asset_store.cpp
	${ENGINE_SOURCES_DIR}/asset_store.h
	${ENGINE_SOURCES_DIR}/game_timer.cpp
	${ENGINE_SOURCES_DIR}/game_timer.h
	${ENGINE_SOURCES_DIR}/application.cpp
	${ENGINE_SOURCES_DIR}/application.h
	${ENGINE_SOURCES_DIR}/application_editor.cpp
	${ENGINE_SOURCES_DIR}/application_editor.h
	${ENGINE_SOURCES_DIR}/editor_commands.cpp
	${ENGINE_SOURCES_DIR}/editor_commands.h
	${ENGINE_SOURCES_DIR}/engine.cpp
	${ENGINE_SOURCES_DIR}/engine_string_impl_def.h
	${ENGINE_SOURCES_DIR}/engine_string.cpp
	${ENGINE_SOURCES_DIR}/engine_vector_impl_def.h
	${ENGINE_SOURCES_DIR}/engine_vector.cpp
	${ENGINE_SOURCES_DIR}/ui_manager.cpp
	${ENGINE_SOURCES_DIR}/ui_manager.h
	${ENGINE_SOURCES_DIR}/ui_document.cpp
	${ENGINE_SOURCES_DIR}/ui_document.h
	${ENGINE_SOURCES_DIR}/logger.cpp
	${ENGINE_SOURCES_DIR}/logger.h
	${ENGINE_SOURCES_DIR}/data_layout.h
	${ENGINE_SOURCES_DIR}/math_helpers.h
	${ENGINE_SOURCES_DIR}/shaders_binding_slots.h
	
	${ENGINE_SOURCES_DIR}/animation_controller.cpp
	${ENGINE_SOURCES_DIR}/animation_controller.h
	
	${ENGINE_SOURCES_DIR}/file_watcher.cpp
	${ENGINE_SOURCES_DIR}/file_watcher.h	
	
	${ENGINE_SOURCES_DIR}/profiler.h
	${ENGINE_SOURCES_DIR}/named_atlas.h
	${ENGINE_SOURCES_DIR}/entity_id_registry.h
	${ENGINE_SOURCES_DIR}/asset_id.h
		
	${ENGINE_SOURCES_DIR}/mesh_defs.h
	${ENGINE_SOURCES_DIR}/gltf_parser.h
	${ENGINE_SOURCES_DIR}/gltf_parser.cpp
	${ENGINE_SOURCES_DIR}/physics_world.h
	${ENGINE_SOURCES_DIR}/physics_world.cpp
	${ENGINE_SOURCES_DIR}/collision_desc.h

	${ENGINE_SOURCES_DIR}/material.h
	${ENGINE_SOURCES_DIR}/material.cpp

	${ENGINE_SOURCES_DIR}/render/render_graph.h
	${ENGINE_SOURCES_DIR}/render/render_graph.cpp
	${ENGINE_SOURCES_DIR}/render/render_graph_gl_backend.h
	${ENGINE_SOURCES_DIR}/render/render_graph_gl_backend.cpp
	${ENGINE_SOURCES_DIR}/render/render_scene_data.h
	${ENGINE_SOURCES_DIR}/render/render_feature.h
	${ENGINE_SOURCES_DIR}/render/render_settings.h
	${ENGINE_SOURCES_DIR}/render/shadows.h
	${ENGINE_SOURCES_DIR}/render/shadows.cpp
	${ENGINE_SOURCES_DIR}/render/ssao_pass.h
	${ENGINE_SOURCES_DIR}/render/ssao_pass.cpp
	${ENGINE_SOURCES_DIR}/render/renderer.h
	${ENGINE_SOURCES_DIR}/render/renderer.cpp
	${ENGINE_SOURCES_DIR}/render/outline_feature.h
	${ENGINE_SOURCES_DIR}/render/outline_feature.cpp
	${ENGINE_SOURCES_DIR}/render/grid_feature.h
	${ENGINE_SOURCES_DIR}/render/grid_feature.cpp

	${ENGINE_SOURCES_DIR}/nav_mesh.h
	${ENGINE_SOURCES_DIR}/nav_mesh.cpp
	${ENGINE_SOURCES_DIR}/world_transform.h
	${ENGINE_SOURCES_DIR}/world_transform.cpp
	${ENGINE_SOURCES_DIR}/physics_system.h
	${ENGINE_SOURCES_DIR}/physics_system.cpp
	${ENGINE_SOURCES_DIR}/nav_mesh_debug_draw.h
	${ENGINE_SOURCES_DIR}/nav_mesh_debug_draw.cpp
	
	${ENGINE_SOURCES_DIR}/skin.h
	${ENGINE_SOURCES_DIR}/skin.cpp
	${ENGINE_SOURCES_DIR}/prefab.h
	${ENGINE_SOURCES_DIR}/prefab.cpp
	${ENGINE_SOURCES_DIR}/serialization/type_registry.h
	${ENGINE_SOURCES_DIR}/serialization/type_registry.cpp
	${ENGINE_SOURCES_DIR}/serialization/builtin_types.h
	${ENGINE_SOURCES_DIR}/serialization/builtin_types.cpp
	${ENGINE_SOURCES_DIR}/serialization/scene_serializer.h
	${ENGINE_SOURCES_DIR}/serialization/scene_serializer.cpp
	${ENGINE_SOURCES_DIR}/serialization/scene_assets.h
	${ENGINE_SOURCES_DIR}/serialization/scene_assets.cpp
	
	${ENGINE_SOURCES_DIR}/components_utils/components_initializers.h
	${ENGINE_SOURCES_DIR}/components_utils/components_initializers.cpp
	
	${ENGINE_SOURCES_DIR}/components_internals/camera_internal_component.h
	${ENGINE_SOURCES_DIR}/components_internals/guizmo_component.h
	${ENGINE_SOURCES_DIR}/components_internals/outline_component.h
	${ENGINE_SOURCES_DIR}/components_internals/physics_component.h
	${ENGINE_SOURCES_DIR}/components_internals/skin_pose_component.h
	${ENGINE_SOURCES_DIR}/components_internals/animation_controller_internal_component.h
	${ENGINE_SOURCES_DIR}/components_internals/prefab_instance_component.h
	${ENGINE_SOURCES_DIR}/components_internals/children_internal_component.h
	${ENGINE_SOURCES_DIR}/components_internals/nav_mesh_internal_component.h
	${ENGINE_SOURCES_DIR}/components_internals/character_controller_internal_component.h
	
	${ENGINE_SOURCES_DIR}/RmlUI_backend/RmlUi_Platform_SDL.h
	${ENGINE_SOURCES_DIR}/RmlUI_backend/RmlUi_Platform_SDL.cpp
	${ENGINE_SOURCES_DIR}/RmlUI_backend/RmlUi_Backend.h
	${ENGINE_SOURCES_DIR}/RmlUI_backend/RmlUi_Renderer_GL3.h
	${ENGINE_SOURCES_DIR}/RmlUI_backend/RmlUi_Renderer_GL3.cpp
	${ENGINE_SOURCES_DIR}/RmlUI_backend/RmlUi_Include_GL3.h
	
	${ENGINE_SOURCES_DIR}/imgui/imgui.h
	${ENGINE_SOURCES_DIR}/imgui/imgui.cpp
	${ENGINE_SOURCES_DIR}/imgui/imconfig.h
	${ENGINE_SOURCES_DIR}/imgui/imgui_internal.h
	${ENGINE_SOURCES_DIR}/imgui/imgui_impl_opengl3.h
	${ENGINE_SOURCES_DIR}/imgui/imgui_impl_opengl3.cpp
	${ENGINE_SOURCES_DIR}/imgui/imgui_impl_opengl3_loader.h
	${ENGINE_SOURCES_DIR}/imgui/imgui_impl_sdl3.h
	${ENGINE_SOURCES_DIR}/imgui/imgui_impl_sdl3.cpp
	${ENGINE_SOURCES_DIR}/imgui/imstb_rectpack.h
	${ENGINE_SOURCES_DIR}/imgui/imstb_textedit.h
	${ENGINE_SOURCES_DIR}/imgui/imstb_truetype.h
	${ENGINE_SOURCES_DIR}/imgui/imgui_widgets.cpp
	${ENGINE_SOURCES_DIR}/imgui/imgui_draw.cpp
	${ENGINE_SOURCES_DIR}/imgui/imgui_tables.cpp
	${ENGINE_SOURCES_DIR}/imgui/imgui_demo.cpp
	
	${ENGINE_SOURCES_DIR}/imguizmo/ImGuizmo.h
	${ENGINE_SOURCES_DIR}/imguizmo/ImGuizmo.cpp
)

set(ENGINE_API "include")
set(ENGINE_API_COMPONENTS ${ENGINE_API}/components)
set(ENGINE_API_SOURCES
	${ENGINE_API}/engine.h
	${ENGINE_API}/engine_defs.h
	${ENGINE_API}/engine_string.h
	${ENGINE_API}/engine_vector.h
	${ENGINE_API_COMPONENTS}/camera_component.h
	${ENGINE_API_COMPONENTS}/light_component.h
	${ENGINE_API_COMPONENTS}/material_component.h
	${ENGINE_API_COMPONENTS}/mesh_component.h
	${ENGINE_API_COMPONENTS}/transform_component.h
	${ENGINE_API_COMPONENTS}/name_component.h
	${ENGINE_API_COMPONENTS}/folder_component.h
	${ENGINE_API_COMPONENTS}/rigid_body_component.h
	${ENGINE_API_COMPONENTS}/collider_component.h
    ${ENGINE_API_COMPONENTS}/parent_component.h
    ${ENGINE_API_COMPONENTS}/nav_surface_component.h
    ${ENGINE_API_COMPONENTS}/nav_modifier_component.h
    ${ENGINE_API_COMPONENTS}/sprite_component.h
    ${ENGINE_API_COMPONENTS}/animation_controller_component.h
    ${ENGINE_API_COMPONENTS}/skin_component.h
)

set(ENGINE_ALL_SOURCES ${ENGINE_SOURCES} ${ENGINE_API_SOURCES})

add_library(${ENGINE} SHARED ${ENGINE_ALL_SOURCES})

target_include_directories(${ENGINE} PUBLIC ${ENGINE_API} PRIVATE ${BULLET_INCLUDE_DIRS} ${ENGINE_SOURCES_DIR}/imgui ${RAPIDJSON_INCLUDE_DIR})
target_link_libraries(${ENGINE} PRIVATE stb tinygltf glad glm EnTT::EnTT SDL3::SDL3-static fmt::fmt-header-only Freetype::Freetype RmlUi::RmlUi RmlUi::Debugger TracyClient ozz_animation ozz_animation_offline RecastNavigation::Recast RecastNavigation::Detour RecastNavigation::DebugUtils ${BULLET_LIBRARIES})
target_compile_definitions(${ENGINE} PUBLIC GLM_FORCE_QUAT_DATA_XYZW GLM_ENABLE_EXPERIMENTAL PRIVATE RMLUI_SDL_VERSION_MAJOR=3 BT_NO_SIMD_OPERATOR_OVERLOADS=1)

target_compile_options(${ENGINE} PRIVATE
  #$<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_ALL_SOURCES})