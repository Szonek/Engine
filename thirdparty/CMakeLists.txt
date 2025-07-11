#sdl
set(SDL_STATIC ON CACH BOOL "")
add_subdirectory(SDL)

#fmt
add_subdirectory(fmt)

# glad
add_subdirectory(glad)

# stb imagr
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "stb")

# glm
add_subdirectory(glm)

# entt
add_subdirectory(entt)

# profiler
add_subdirectory(tracy-0.12.1)

# gtests
add_subdirectory(googletest)

# skeleton animations
set(ozz_build_tools OFF CACHE BOOL "")
set(ozz_build_fbx OFF CACHE BOOL "")
set(ozz_build_gltf ON CACHE BOOL "")
set(ozz_build_samples OFF CACHE BOOL "")
set(ozz_build_howtos OFF CACHE BOOL "")
set(ozz_build_tests OFF CACHE BOOL "")
set(ozz_build_msvc_rt_dll ON CACHE BOOL "")
add_subdirectory(ozz-animation)

# rapidJson
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "")
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(rapidjson)

# gltf 2.0
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "")
set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "")
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "")
set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF CACHE BOOL "")
set(TINYGLTF_INSTALL OFF CACHE BOOL "")
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "")
add_subdirectory(tinygltf)

#bullet3
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "")
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "")
set(BUILD_CPU_DEMOS OFF CACHE BOOL "")
set(BUILD_UNIT_TESTS OFF CACHE BOOL "")
set(BUILD_ENET OFF CACHE BOOL "")
set(BUILD_CLSOCKET OFF CACHE BOOL "")
set(BUILD_EXTRAS OFF CACHE BOOL "")
add_subdirectory(bullet3)
set(BULLET_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src  CACHE STRING "Include headers of bullet dependency")
set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath CACHE STRING "Include libraries of bullet dependency")

#freetype prebuilt
add_library(Freetype::Freetype STATIC IMPORTED GLOBAL)
set_property(TARGET Freetype::Freetype PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/freetype/include)
set_property(TARGET Freetype::Freetype PROPERTY IMPORTED_LOCATION_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/freetype/DebugStatic/freetype.lib)
set_property(TARGET Freetype::Freetype PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/freetype/ReleaseStatic/freetype.lib)

#RML UI 
set(RMLUI_SAMPLES ON CACHE BOOL "Build samples of RmlUI thirdparty")
set(RMLUI_BACKEND Win32_GL2 CACHE STRING "")
set(BUILD_TESTING OFF CACHE BOOL "Build tests of RmlUI thirdparty")
set(NO_FONT_INTERFACE_DEFAULT OFF CACHE BOOL "Do not include default font interface for RmUI thirdparty.")
add_subdirectory(RmlUi)

# set some of the deps as SYSTEM to not report warnings while compiling them
set_target_properties(tinygltf PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:tinygltf,INTERFACE_INCLUDE_DIRECTORIES>)
# set_target_properties(glad PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:glad,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(glm PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(stb PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:stb,INTERFACE_INCLUDE_DIRECTORIES>)