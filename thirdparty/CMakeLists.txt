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

#bullet3
add_subdirectory(bullet3)
set(BULLET_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src  CACHE STRING "Include headers of bullet dependency")
set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath CACHE STRING "Include libraries of bullet dependency")

# font library
add_subdirectory(freetype)
