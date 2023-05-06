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

# gltf 2.0
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF)
set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF)
set(TINYGLTF_INSTALL OFF)
set(TINYGLTF_HEADER_ONLY ON)
add_subdirectory(tinygltf)

#bullet3
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
set(BUILD_OPENGL3_DEMOS OFF)
set(BUILD_BULLET2_DEMOS OFF)
set(BUILD_CPU_DEMOS OFF)
set(BUILD_UNIT_TESTS OFF)
endif()
add_subdirectory(bullet3)
set(BULLET_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src  CACHE STRING "Include headers of bullet dependency")
set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath CACHE STRING "Include libraries of bullet dependency")

# font library
add_subdirectory(freetype)
