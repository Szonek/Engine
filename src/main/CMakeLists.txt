set(APP_NAME "main")

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    find_library(log-lib log)

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

    add_library(${APP_NAME} SHARED main.cpp)
    target_link_libraries(${APP_NAME}
            android
            ${log-lib})
else ()
    add_executable(${APP_NAME} main.cpp)
endif()
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${APP_NAME} engine fmt::fmt-header-only SDL3::SDL3-static glm)
