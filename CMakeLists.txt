# EnTT 4.0 requires 3.28
cmake_minimum_required(VERSION 3.28)

set(TARGET_NAME "OpenGLPlayground")
project(${TARGET_NAME} CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)
# static thirdparty libraries are linked into the engine shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

add_subdirectory(thirdparty)
enable_testing()
add_subdirectory(src)
