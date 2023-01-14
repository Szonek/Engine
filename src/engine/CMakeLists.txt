set(ENGINE "engine")

set(ENGINE_SOURCES
	impl/graphics.cpp
	impl/graphics.h
	impl/scene.cpp
	impl/scene.h
	impl/asset_store.cpp
	impl/asset_store.h
	impl/game_timer.cpp
	impl/game_timer.h
	impl/application.cpp
	impl/application.h
	impl/engine.cpp
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
)

set(ENGINE_ALL_SOURCES ${ENGINE_SOURCES} ${ENGINE_API_SOURCES})

add_library(${ENGINE} SHARED ${ENGINE_ALL_SOURCES})
set_property(TARGET ${ENGINE} PROPERTY CXX_STANDARD 20)
target_include_directories(${ENGINE} PUBLIC ${ENGINE_API})
target_link_libraries(${ENGINE} stb glad glfw glm EnTT::EnTT ${GLFW_LIBRARIES})


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_ALL_SOURCES})