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
	${ENGINE_SOURCES_DIR}/engine.cpp
	${ENGINE_SOURCES_DIR}/ui_manager.cpp
	${ENGINE_SOURCES_DIR}/ui_manager.h
	${ENGINE_SOURCES_DIR}/logger.cpp
	${ENGINE_SOURCES_DIR}/logger.h
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
	${ENGINE_API_COMPONENTS}/rect_transform_component.h
	${ENGINE_API_COMPONENTS}/name_component.h
	${ENGINE_API_COMPONENTS}/text_component.h
	${ENGINE_API_COMPONENTS}/rigid_body_component.h
	${ENGINE_API_COMPONENTS}/collider_component.h
      ${ENGINE_API_COMPONENTS}/image_component.h
)

set(ENGINE_ALL_SOURCES ${ENGINE_SOURCES} ${ENGINE_API_SOURCES})

add_library(${ENGINE} SHARED ${ENGINE_ALL_SOURCES})
set_property(TARGET ${ENGINE} PROPERTY CXX_STANDARD 20)


target_include_directories(${ENGINE} PUBLIC ${ENGINE_API} PRIVATE ${BULLET_INCLUDE_DIRS})
target_link_libraries(${ENGINE} PRIVATE stb glad glm EnTT::EnTT freetype SDL3::SDL3-static fmt::fmt-header-only ${BULLET_LIBRARIES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_ALL_SOURCES})