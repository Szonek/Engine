set(APP_NAME "project_c_game")

set(APP_SOURCES
	main.cpp
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
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${APP_ALL_SOURCES})