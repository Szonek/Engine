cmake_minimum_required(VERSION 3.20.0)

set(TARGET_NAME "OpenGLPlayground")
project(${TARGET_NAME} CXX)

add_executable(${TARGET_NAME} main.cpp)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
