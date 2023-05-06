set(APP_NAME "main")

set(APP_SOURCES
	main.cpp
)

set(APP_TOOLKIT_DIR "app_toolkit")
set(APP_TOOLKIT_SOURCES
	${APP_TOOLKIT_DIR}/utils.h
	${APP_TOOLKIT_DIR}/iscript.h
	${APP_TOOLKIT_DIR}/iscene.h
	${APP_TOOLKIT_DIR}/iscene.cpp
	${APP_TOOLKIT_DIR}/gui_event_system.h
	${APP_TOOLKIT_DIR}/gui_event_system.cpp
)

set(GAME_DIR "pong")

set(GAME_SCRIPTS_DIR ${GAME_DIR}/scripts)
set(GAME_SCRIPTS_SOURCES
	${GAME_SCRIPTS_DIR}/ball_script.h
	${GAME_SCRIPTS_DIR}/ball_script.cpp
	${GAME_SCRIPTS_DIR}/global_constants.h
	${GAME_SCRIPTS_DIR}/goal_net_script.h
	${GAME_SCRIPTS_DIR}/goal_net_script.cpp
	${GAME_SCRIPTS_DIR}/player_paddle_script.h
	${GAME_SCRIPTS_DIR}/player_paddle_script.cpp
	${GAME_SCRIPTS_DIR}/wall_script.h
	${GAME_SCRIPTS_DIR}/wall_script.cpp
	${GAME_SCRIPTS_DIR}/camera_script.h
	${GAME_SCRIPTS_DIR}/camera_script.cpp
)
set(GAME_SCENES_DIR ${GAME_DIR}/scenes)
set(GAME_SCENES_SOURCES
	${GAME_SCENES_DIR}/main_scene.h
	${GAME_SCENES_DIR}/main_scene.cpp
	${GAME_SCENES_DIR}/main_menu_scene.h
	${GAME_SCENES_DIR}/main_menu_scene.cpp
)

set(APP_ALL_SOURCES ${APP_SOURCES} ${APP_TOOLKIT_SOURCES} ${GAME_SCRIPTS_SOURCES} ${GAME_SCENES_SOURCES})

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
target_link_libraries(${APP_NAME} engine fmt::fmt-header-only SDL3::SDL3-static glm)
target_include_directories(${APP_NAME} PUBLIC ${APP_TOOLKIT_DIR})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${APP_ALL_SOURCES})