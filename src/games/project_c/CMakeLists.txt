set(APP_NAME "project_c_game")
# all gameplay code except main(), so tests (scenario_tests) can link it
set(LIB_NAME "project_c_lib")

set(LIB_SOURCES
	prefab_types.h
	
	app.h
	app.cpp

	player_input.h
	player_input.cpp
	
	
	scenes/level_builder.h
	scenes/level_builder.cpp
	scenes/level_scene.h
	scenes/level_scene.cpp
	
	scripts/base_script.h
	scripts/base_script.cpp
	scripts/building_script.h
	scripts/building_script.cpp
	scripts/camera_script.h
	scripts/camera_script.cpp
	scripts/interactable_script.h
	scripts/interactable_script.cpp
	scripts/coin_script.h
	scripts/coin_script.cpp
	scripts/encounter_script.h
	scripts/encounter_script.cpp
	scripts/enemy_script.h
	scripts/enemy_script.cpp
	scripts/enviorment_script.h
	scripts/enviorment_script.cpp
	scripts/game_components.h
	scripts/game_components.cpp
	scripts/gameplay_hud_script.h
	scripts/gameplay_hud_script.cpp
	scripts/player_script.h
	scripts/player_script.cpp
	
	scripts/scripts_utils.h
	scripts/scripts_utils.cpp
)

set(APP_SOURCES
	main.cpp
)

add_library(${LIB_NAME} STATIC ${LIB_SOURCES})
set_target_properties(${LIB_NAME} PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${LIB_NAME} PUBLIC engine fmt::fmt-header-only glm engine_app_toolkit RapidJSON)
target_compile_definitions(${LIB_NAME} PUBLIC GLM_FORCE_QUAT_DATA_XYZW)
# runtime asset directory: the assets folder of this source tree
target_compile_definitions(${LIB_NAME} PRIVATE PROJECT_C_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${LIB_SOURCES})

add_executable(${APP_NAME} ${APP_SOURCES})
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${APP_NAME} ${LIB_NAME})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${APP_SOURCES})