set(APP_NAME "app")
add_executable(${APP_NAME} main.cpp)
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${APP_NAME} engine ${GLFW_LIBRARIES})
