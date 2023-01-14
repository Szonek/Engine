set(APP_NAME "main")

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    #add_library(native_app_glue STATIC
    #        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    #target_include_directories(native_app_glue PUBLIC
    #        ${ANDROID_NDK}/sources/android/native_app_glue)

    find_library(log-lib log)

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

    add_library(${APP_NAME} SHARED main.cpp)
    target_link_libraries(${APP_NAME}
            android
            #native_app_glue
            ${log-lib})
else ()
    add_executable(${APP_NAME} main.cpp)
endif()
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${APP_NAME} engine fmt::fmt-header-only)
