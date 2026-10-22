# Only the libraries are built: every dependency's samples, examples, tests,
# benchmarks, tools and docs are turned off.

#sdl
set(SDL_STATIC ON CACHE BOOL "")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
set(SDL_TESTS OFF CACHE BOOL "")
set(SDL_EXAMPLES OFF CACHE BOOL "")
add_subdirectory(SDL)

#fmt
set(FMT_TEST OFF CACHE BOOL "")
set(FMT_DOC OFF CACHE BOOL "")
add_subdirectory(fmt)

# glad
add_subdirectory(glad)

# stb imagr
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "stb")

# glm
set(GLM_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(glm)

# entt
set(ENTT_BUILD_TESTING OFF CACHE BOOL "")
set(ENTT_BUILD_EXAMPLE OFF CACHE BOOL "")
set(ENTT_BUILD_BENCHMARK OFF CACHE BOOL "")
set(ENTT_BUILD_TOOLS OFF CACHE BOOL "")
set(ENTT_BUILD_DOCS OFF CACHE BOOL "")
add_subdirectory(entt)

# profiler
add_subdirectory(tracy-0.12.1)

# gtests (the framework of our own tests)
set(INSTALL_GTEST OFF CACHE BOOL "")
set(BUILD_GMOCK OFF CACHE BOOL "")
add_subdirectory(googletest)

# skeleton animations
set(ozz_build_tools OFF CACHE BOOL "")
set(ozz_build_fbx OFF CACHE BOOL "")
set(ozz_build_gltf OFF CACHE BOOL "")
set(ozz_build_samples OFF CACHE BOOL "")
set(ozz_build_howtos OFF CACHE BOOL "")
set(ozz_build_tests OFF CACHE BOOL "")
set(ozz_build_msvc_rt_dll ON CACHE BOOL "")
add_subdirectory(ozz-animation)

# rapidJson
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "")
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "")
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "")
add_subdirectory(rapidjson)
# header-only, without a target
set(RAPIDJSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/include CACHE INTERNAL "")

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
set(BUILD_PYBULLET OFF CACHE BOOL "")
set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "")
add_subdirectory(bullet3)
set(BULLET_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src  CACHE STRING "Include headers of bullet dependency")
set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath CACHE STRING "Include libraries of bullet dependency")

# navigation: Recast (nav mesh generation), Detour (path queries), DebugUtils (nav mesh debug draw)
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "")
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "")
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "")
# its cmake_minimum_required(VERSION 3.1) is below what CMake 4 still accepts (3.5); older CMake ignores this
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(recastnavigation)
unset(CMAKE_POLICY_VERSION_MINIMUM)

#freetype: prebuilt on Windows, the system package elsewhere
if(WIN32)
add_library(Freetype::Freetype STATIC IMPORTED GLOBAL)
set_property(TARGET Freetype::Freetype PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/freetype/include)
set_property(TARGET Freetype::Freetype PROPERTY IMPORTED_LOCATION_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/freetype/DebugStatic/freetype.lib)
set_property(TARGET Freetype::Freetype PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/freetype/ReleaseStatic/freetype.lib)
else()
find_package(Freetype REQUIRED)
set_target_properties(Freetype::Freetype PROPERTIES IMPORTED_GLOBAL TRUE)
endif()

#RML UI (the engine has its own backend, impl/RmlUI_backend)
set(RMLUI_SAMPLES OFF CACHE BOOL "")
set(BUILD_TESTING OFF CACHE BOOL "")
set(NO_FONT_INTERFACE_DEFAULT OFF CACHE BOOL "Do not include default font interface for RmUI thirdparty.")
add_subdirectory(RmlUi)

# set some of the deps as SYSTEM to not report warnings while compiling them
set_target_properties(tinygltf PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:tinygltf,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(glad PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:glad,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(glm PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(stb PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:stb,INTERFACE_INCLUDE_DIRECTORIES>)
foreach(dep EnTT fmt-header-only ozz_base ozz_animation ozz_animation_offline)
  set_target_properties(${dep} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:${dep},INTERFACE_INCLUDE_DIRECTORIES>)
endforeach()
