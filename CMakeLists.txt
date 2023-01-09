cmake_minimum_required(VERSION 3.20.0)

set(TARGET_NAME "OpenGLPlayground")
project(${TARGET_NAME} CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)


add_subdirectory(thirdparty)
add_subdirectory(src)
