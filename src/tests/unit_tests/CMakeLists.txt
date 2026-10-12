set(APP_NAME "unit_tests")

set(APP_SOURCES
	main.cpp
	
	basic_tests.cpp
)
add_executable(${APP_NAME} ${APP_SOURCES})
target_link_libraries(${APP_NAME} GTest::gtest_main engine engine_app_toolkit glm)

add_test(NAME ${APP_NAME} COMMAND ${APP_NAME})

# Render graph logic doesn't need a graphics API (nor the engine library), so it's compiled directly into its own test.
set(RENDER_GRAPH_TESTS_NAME "render_graph_tests")
set(ENGINE_IMPL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/impl)
add_executable(${RENDER_GRAPH_TESTS_NAME}
	render_graph_tests.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.h
)
target_include_directories(${RENDER_GRAPH_TESTS_NAME} PRIVATE ${ENGINE_IMPL_DIR})
target_link_libraries(${RENDER_GRAPH_TESTS_NAME} GTest::gtest_main)
add_test(NAME ${RENDER_GRAPH_TESTS_NAME} COMMAND ${RENDER_GRAPH_TESTS_NAME})