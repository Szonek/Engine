# Tests labelled "ci" need no GPU and run in CI (.github/workflows). Tests that need a GL context must not get the label.

# Engine logic that doesn't need a graphics API (nor the engine library): the sources under test are compiled directly
# into this executable. Add new GL-free tests here rather than creating another executable.
set(ENGINE_LOGIC_TESTS_NAME "engine_logic_tests")
set(ENGINE_IMPL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/impl)
add_executable(${ENGINE_LOGIC_TESTS_NAME}
	render_graph_tests.cpp
	shadows_tests.cpp
	atlas_tests.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.h
	${ENGINE_IMPL_DIR}/render/shadows.cpp
	${ENGINE_IMPL_DIR}/render/shadows.h
	# header-only; atlas_tests.cpp provides the logger it uses
	${ENGINE_IMPL_DIR}/named_atlas.h
)
target_include_directories(${ENGINE_LOGIC_TESTS_NAME} PRIVATE ${ENGINE_IMPL_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include)
target_link_libraries(${ENGINE_LOGIC_TESTS_NAME} GTest::gtest_main glm fmt::fmt-header-only)
add_test(NAME ${ENGINE_LOGIC_TESTS_NAME} COMMAND ${ENGINE_LOGIC_TESTS_NAME})
set_tests_properties(${ENGINE_LOGIC_TESTS_NAME} PROPERTIES LABELS ci)
