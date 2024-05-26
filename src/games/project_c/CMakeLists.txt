set(APP_NAME "project_c_game")

set(APP_SOURCES
	main.cpp
	model_info.h
	
	app.h
	app.cpp
	
	scenes/scene_city.h
	scenes/scene_city.cpp	
	scenes/scene_test.h
	scenes/scene_test.cpp
	
	scripts/base_script.h
	scripts/base_script.cpp
	scripts/camera_script.h
	scripts/camera_script.cpp
	scripts/enemy_script.h
	scripts/enemy_script.cpp
	scripts/enviorment_script.h
	scripts/enviorment_script.cpp
	scripts/solider_script.h
	scripts/solider_script.cpp
	
	scripts/scripts_utils.h
	scripts/scripts_utils.cpp
)


set(APP_ALL_SOURCES ${APP_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    find_library(log-lib log)

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

    add_library(${APP_NAME} SHARED ${APP_ALL_SOURCES})
    target_link_libraries(${APP_NAME}
            android
            ${log-lib})
else ()
    add_executable(${APP_NAME} ${APP_ALL_SOURCES})
endif()
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${APP_NAME} engine fmt::fmt-header-only SDL3::SDL3-static glm asio engine_app_toolkit RapidJSON)
target_compile_definitions(${APP_NAME} PUBLIC GLM_FORCE_QUAT_DATA_XYZW)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${APP_ALL_SOURCES})