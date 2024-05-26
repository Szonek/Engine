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
	${ENGINE_SOURCES_DIR}/engine.cpp
	${ENGINE_SOURCES_DIR}/ui_manager.cpp
	${ENGINE_SOURCES_DIR}/ui_manager.h
	${ENGINE_SOURCES_DIR}/ui_document.cpp
	${ENGINE_SOURCES_DIR}/ui_document.h
	${ENGINE_SOURCES_DIR}/logger.cpp
	${ENGINE_SOURCES_DIR}/logger.h
	${ENGINE_SOURCES_DIR}/profiler.h
	${ENGINE_SOURCES_DIR}/gltf_parser.h
	${ENGINE_SOURCES_DIR}/gltf_parser.cpp
	${ENGINE_SOURCES_DIR}/physics_world.h
	${ENGINE_SOURCES_DIR}/physics_world.cpp
	
	${ENGINE_SOURCES_DIR}/animation_controller.h
	${ENGINE_SOURCES_DIR}/animation_controller.cpp
	
	${ENGINE_SOURCES_DIR}/components_utils/components_initializers.h
	${ENGINE_SOURCES_DIR}/components_utils/components_initializers.cpp
	
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
)

set(ENGINE_API "include")
set(ENGINE_API_COMPONENTS ${ENGINE_API}/components)
set(ENGINE_API_SOURCES
	${ENGINE_API}/engine.h
	${ENGINE_API_COMPONENTS}/camera_component.h
	${ENGINE_API_COMPONENTS}/light_component.h
	${ENGINE_API_COMPONENTS}/material_component.h
	${ENGINE_API_COMPONENTS}/mesh_component.h
	${ENGINE_API_COMPONENTS}/transform_component.h
	${ENGINE_API_COMPONENTS}/name_component.h
	${ENGINE_API_COMPONENTS}/rigid_body_component.h
	${ENGINE_API_COMPONENTS}/collider_component.h
    ${ENGINE_API_COMPONENTS}/parent_component.h
)

set(ENGINE_ALL_SOURCES ${ENGINE_SOURCES} ${ENGINE_API_SOURCES})

add_library(${ENGINE} SHARED ${ENGINE_ALL_SOURCES})

target_include_directories(${ENGINE} PUBLIC ${ENGINE_API} PRIVATE ${BULLET_INCLUDE_DIRS})
target_link_libraries(${ENGINE} PRIVATE stb tinygltf glad glm EnTT::EnTT SDL3::SDL3-static fmt::fmt-header-only RmlCore TracyClient ${BULLET_LIBRARIES})
target_compile_definitions(${ENGINE} PUBLIC GLM_FORCE_QUAT_DATA_XYZW GLM_ENABLE_EXPERIMENTAL)

get_target_property(target_type RmlCore TYPE)
if (target_type STREQUAL "STATIC_LIBRARY")
	target_compile_definitions(${ENGINE} PRIVATE RMLUI_STATIC_LIB)
endif ()

target_compile_options(${ENGINE} PRIVATE
  #$<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_ALL_SOURCES})