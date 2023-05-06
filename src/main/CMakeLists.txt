set(APP_NAME "main")

set(APP_SOURCES
	main.cpp
	utils.h
	iscript.h
	scene.h
	gui_event_system.h
	gui_event_system.cpp
)

set(GAME_DIR "pong")
set(GAME_SOURCES
	${GAME_DIR}/ball_script.h
	${GAME_DIR}/ball_script.cpp
	${GAME_DIR}/global_constants.h
	${GAME_DIR}/goal_net_script.h
	${GAME_DIR}/player_paddle_script.h
	${GAME_DIR}/player_paddle_script.cpp
	${GAME_DIR}/wall_script.h
)

set(ALL_SOURCES ${APP_SOURCES} ${GAME_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    find_library(log-lib log)

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

    add_library(${APP_NAME} SHARED ${ALL_SOURCES})
    target_link_libraries(${APP_NAME}
            android
            ${log-lib})
else ()
    add_executable(${APP_NAME} ${ALL_SOURCES})
endif()
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${APP_NAME} engine fmt::fmt-header-only SDL3::SDL3-static glm)
