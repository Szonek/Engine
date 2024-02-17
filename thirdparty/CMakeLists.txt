#sdl
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

# entt
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "asio/asio/include")

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
add_subdirectory(bullet3)
set(BULLET_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src  CACHE STRING "Include headers of bullet dependency")
set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath CACHE STRING "Include libraries of bullet dependency")

# UI library
set(BUILD_SAMPLES OFF CACHE BOOL "Build samples of RmlUI thirdparty")
set(NO_FONT_INTERFACE_DEFAULT OFF CACHE BOOL "Do not include default font interface for RmUI thirdparty.")
add_subdirectory(RmlUi)
