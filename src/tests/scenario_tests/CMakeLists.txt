# Scenario tests run real gameplay code (project_c_lib) in a hidden window with a GL context.
# CI runners have no GPU, so these are labelled "gpu", not "ci".
set(SCENARIO_TESTS_NAME "scenario_tests")
add_executable(${SCENARIO_TESTS_NAME}
	main.cpp
	scenario_harness.h
	scenario_harness.cpp
	sandbox_scene.h
	sandbox_scene.cpp

	animation_controller_tests.cpp
	asset_name_tests.cpp
	gameplay_tests.cpp
	harness_tests.cpp
	level_tests.cpp
	physics_tests.cpp
)
set_property(TARGET ${SCENARIO_TESTS_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${SCENARIO_TESTS_NAME} GTest::gtest project_c_lib)
add_test(NAME ${SCENARIO_TESTS_NAME} COMMAND ${SCENARIO_TESTS_NAME})
set_tests_properties(${SCENARIO_TESTS_NAME} PROPERTIES LABELS gpu TIMEOUT 120)
