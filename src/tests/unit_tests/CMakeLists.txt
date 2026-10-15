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
# The same is true for the math of the shadow map.
add_executable(${RENDER_GRAPH_TESTS_NAME}
	render_graph_tests.cpp
	shadows_tests.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.h
	${ENGINE_IMPL_DIR}/render/shadows.cpp
	${ENGINE_IMPL_DIR}/render/shadows.h
)
target_include_directories(${RENDER_GRAPH_TESTS_NAME} PRIVATE ${ENGINE_IMPL_DIR})
target_link_libraries(${RENDER_GRAPH_TESTS_NAME} GTest::gtest_main glm)
add_test(NAME ${RENDER_GRAPH_TESTS_NAME} COMMAND ${RENDER_GRAPH_TESTS_NAME})
# The resource atlas is header-only; the test provides its own logger, so neither GL nor the engine library is needed.
set(ATLAS_TESTS_NAME "atlas_tests")
add_executable(${ATLAS_TESTS_NAME}
	atlas_tests.cpp
	${ENGINE_IMPL_DIR}/named_atlas.h
)
target_include_directories(${ATLAS_TESTS_NAME} PRIVATE ${ENGINE_IMPL_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include)
target_link_libraries(${ATLAS_TESTS_NAME} GTest::gtest_main fmt::fmt-header-only)
add_test(NAME ${ATLAS_TESTS_NAME} COMMAND ${ATLAS_TESTS_NAME})
