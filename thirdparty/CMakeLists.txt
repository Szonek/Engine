# glfw
add_subdirectory(glfw)
include_directories("glfw/include")

# glad
add_subdirectory(glad)

# stb imagr
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "stb")

# glm
add_subdirectory(glm)

# entt
add_subdirectory(entt)