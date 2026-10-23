# Benchmarks run real gameplay code (project_c_lib) in a visible window with vsync off and report frame times.
# They need a GPU and are not tests: no add_test, so neither CI nor ctest runs them.
set(BIG_SCENE_BENCHMARK_NAME "big_scene_benchmark")
add_executable(${BIG_SCENE_BENCHMARK_NAME}
	big_scene_benchmark.cpp
)
set_property(TARGET ${BIG_SCENE_BENCHMARK_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(${BIG_SCENE_BENCHMARK_NAME} project_c_lib)
