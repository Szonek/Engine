# Tests labelled "ci" need no GPU and run in CI (.github/workflows). Tests that need a GL context must not get the label.

# Engine logic that doesn't need a graphics API (nor the engine library): the sources under test are compiled directly
# into this executable. Add new GL-free tests here rather than creating another executable.
set(ENGINE_LOGIC_TESTS_NAME "engine_logic_tests")
set(ENGINE_IMPL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/impl)
add_executable(${ENGINE_LOGIC_TESTS_NAME}
	test_logger.cpp
	test_logger.h
	render_graph_tests.cpp
	shadows_tests.cpp
	pixel_art_tests.cpp
	atlas_tests.cpp
	entity_id_registry_tests.cpp
	asset_id_tests.cpp
	skin_tests.cpp
	nav_mesh_tests.cpp
	world_transform_tests.cpp
	hierarchy_tests.cpp
	type_registry_tests.cpp
	scene_serializer_tests.cpp
	game_types_tests.cpp
	data_asset_tests.cpp
	copy_name_tests.cpp
	editor_camera_tests.cpp
	editor_modal_transform_tests.cpp
	editor_asset_browser_tests.cpp
	play_mode_tests.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.cpp
	${ENGINE_IMPL_DIR}/render/render_graph.h
	${ENGINE_IMPL_DIR}/render/shadows.cpp
	${ENGINE_IMPL_DIR}/render/shadows.h
	${ENGINE_IMPL_DIR}/render/pixel_art.cpp
	${ENGINE_IMPL_DIR}/render/pixel_art.h
	# header-only; test_logger.cpp provides the logger the engine sources use
	${ENGINE_IMPL_DIR}/named_atlas.h
	# header-only
	${ENGINE_IMPL_DIR}/entity_id_registry.h
	# header-only: how the editor refers to game objects across a change of their handles
	${ENGINE_IMPL_DIR}/game_object_ref.h
	# header-only: the names of the editor's copies
	${ENGINE_IMPL_DIR}/copy_name.h
	# skin asset + pose and the animation controller (ozz only, no GL)
	${ENGINE_IMPL_DIR}/skin.cpp
	${ENGINE_IMPL_DIR}/skin.h
	${ENGINE_IMPL_DIR}/animation_controller.cpp
	${ENGINE_IMPL_DIR}/animation_controller.h
	${ENGINE_IMPL_DIR}/asset_id.h
	# nav mesh build (Recast) and queries (Detour)
	${ENGINE_IMPL_DIR}/nav_mesh.cpp
	${ENGINE_IMPL_DIR}/nav_mesh.h
	# local TRS -> world matrices (EnTT registry only)
	${ENGINE_IMPL_DIR}/world_transform.cpp
	${ENGINE_IMPL_DIR}/world_transform.h
	# the children lists kept from the parent components (EnTT registry only)
	${ENGINE_IMPL_DIR}/hierarchy.cpp
	${ENGINE_IMPL_DIR}/hierarchy.h
	${ENGINE_IMPL_DIR}/parent_storage.h
	# the editor's viewport camera and modal transform (glm only)
	${ENGINE_IMPL_DIR}/editor_camera.cpp
	${ENGINE_IMPL_DIR}/editor_camera.h
	${ENGINE_IMPL_DIR}/editor_modal_transform.cpp
	${ENGINE_IMPL_DIR}/editor_modal_transform.h
	${ENGINE_IMPL_DIR}/editor_snap.h
	# what the editor's asset browser lists and how it names what it drags (std::filesystem + glm)
	${ENGINE_IMPL_DIR}/editor_asset_browser.cpp
	${ENGINE_IMPL_DIR}/editor_asset_browser.h
	# the play state and the snapshot taken at Play (the caller saves and loads the scene)
	${ENGINE_IMPL_DIR}/play_mode.cpp
	${ENGINE_IMPL_DIR}/play_mode.h
	# type registry, the engine's component descriptions, game-defined types and scene files (EnTT registry + rapidjson)
	${ENGINE_IMPL_DIR}/serialization/type_registry.cpp
	${ENGINE_IMPL_DIR}/serialization/type_registry.h
	${ENGINE_IMPL_DIR}/serialization/builtin_types.cpp
	${ENGINE_IMPL_DIR}/serialization/builtin_types.h
	${ENGINE_IMPL_DIR}/serialization/game_types.cpp
	${ENGINE_IMPL_DIR}/serialization/game_types.h
	${ENGINE_IMPL_DIR}/serialization/scene_serializer.cpp
	${ENGINE_IMPL_DIR}/serialization/scene_serializer.h
	${ENGINE_IMPL_DIR}/serialization/prefab_instances.h
	# data assets and the file watcher they register with (its thread is never started here)
	${ENGINE_IMPL_DIR}/data_asset.cpp
	${ENGINE_IMPL_DIR}/data_asset.h
	${ENGINE_IMPL_DIR}/file_watcher.cpp
	${ENGINE_IMPL_DIR}/file_watcher.h
)
target_include_directories(${ENGINE_LOGIC_TESTS_NAME} PRIVATE ${ENGINE_IMPL_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include ${RAPIDJSON_INCLUDE_DIR})
# profiler.h includes Tracy and RmlUi headers: only their headers, without TRACY_ENABLE the profiling macros are empty
target_include_directories(${ENGINE_LOGIC_TESTS_NAME} PRIVATE
	$<TARGET_PROPERTY:TracyClient,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:RmlUi::RmlUi,INTERFACE_INCLUDE_DIRECTORIES>)
# same glm configuration as the engine, so quaternions have the same layout
target_compile_definitions(${ENGINE_LOGIC_TESTS_NAME} PRIVATE GLM_FORCE_QUAT_DATA_XYZW GLM_ENABLE_EXPERIMENTAL)
# the engine's parent component storage, force-included like in the engine (see its CMakeLists)
target_compile_options(${ENGINE_LOGIC_TESTS_NAME} PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:$<IF:$<CXX_COMPILER_ID:MSVC>,/FI${ENGINE_IMPL_DIR}/parent_storage.h,SHELL:-include ${ENGINE_IMPL_DIR}/parent_storage.h>>")
target_link_libraries(${ENGINE_LOGIC_TESTS_NAME} GTest::gtest_main glm fmt::fmt-header-only EnTT::EnTT ozz_animation ozz_animation_offline RecastNavigation::Recast RecastNavigation::Detour)
add_test(NAME ${ENGINE_LOGIC_TESTS_NAME} COMMAND ${ENGINE_LOGIC_TESTS_NAME})
set_tests_properties(${ENGINE_LOGIC_TESTS_NAME} PROPERTIES LABELS ci)
