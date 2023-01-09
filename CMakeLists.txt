cmake_minimum_required(VERSION 3.20.0)

set(TARGET_NAME "OpenGLPlayground")
project(${TARGET_NAME} CXX)

add_subdirectory(thirdparty)
add_subdirectory(src)
