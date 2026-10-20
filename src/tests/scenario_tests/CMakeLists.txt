# Scenario tests run real gameplay code (project_c_lib) in a hidden window with a GL context.
# CI runners have no GPU, so these are labelled "gpu", not "ci".
set(SCENARIO_TESTS_NAME "scenario_tests")
add_executable(${SCENARIO_TESTS_NAME}
	main.cpp
	scenario_harness.h
	scenario_harness.cpp
	sandbox_scene.h
	sandbox_scene.cpp
	test_models.h

	animation_controller_tests.cpp
	asset_name_tests.cpp
	character_asset_tests.cpp
	character_controller_tests.cpp
	gameplay_tests.cpp
	hierarchy_tests.cpp
	harness_tests.cpp
	level_tests.cpp
	physics_tests.cpp
	prefab_tests.cpp
	script_lifecycle_tests.cpp
)
set_property(TARGET ${SCENARIO_TESTS_NAME} PROPERTY CXX_STANDARD 20)
# the assets folder of this source tree, for tests which load a file by its path
target_compile_definitions(${SCENARIO_TESTS_NAME} PRIVATE TEST_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
target_link_libraries(${SCENARIO_TESTS_NAME} GTest::gtest project_c_lib)
add_test(NAME ${SCENARIO_TESTS_NAME} COMMAND ${SCENARIO_TESTS_NAME})
set_tests_properties(${SCENARIO_TESTS_NAME} PROPERTIES LABELS gpu TIMEOUT 120)
