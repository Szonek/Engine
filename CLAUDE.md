# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A hobby C++20 game engine ("Engine") plus the ARPG prototype built on it ("Project C"). OpenGL renderer, SDL3 windowing/input, EnTT ECS, Bullet physics, ozz-animation skeletal animation, RmlUi game UI, ImGui/ImGuizmo editor, Tracy profiling.

## Build, run, test

Windows + MSVC is the only maintained desktop configuration (FreeType is linked from prebuilt `.lib` files in `thirdparty/freetype`). Dependencies are git submodules: `git submodule update --init --recursive`.

The local build tree is `build/` (Visual Studio 17 2022, x64, multi-config). CI and the README use `buildtree/` with NMake. Both are gitignored.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64          # configure
cmake --build build --config Debug --target project_c_game     # build the game (+ engine, toolkit)
cmake --build build --config Debug                             # build everything, including all thirdparty samples (slow)
build\bin\Debug\project_c_game.exe                             # run

cmake --build build --config Debug --target engine_logic_tests scenario_tests
ctest --test-dir build -C Debug                                # all tests
ctest --test-dir build -C Debug -L ci                          # only the tests CI runs
ctest --test-dir build -C Debug -L gpu --output-on-failure     # scenario tests (real game code, needs a GPU)
build\bin\Debug\engine_logic_tests.exe --gtest_filter=Suite.Name   # single test
```

Binaries land in `build/bin/<Config>/`, libraries in `build/lib/<Config>/`.

Things that will trip you up:

- **Sources are listed explicitly** in each `CMakeLists.txt` (no globbing). A new `.cpp`/`.h` must be added to the list by hand. `src/games/project_c/scripts/solider_script.cpp` is on disk but intentionally not in the build.
- **The asset path is baked in at configure time**: `PROJECT_C_ASSETS_DIR` (set in `src/games/project_c/CMakeLists.txt`) points at the `assets/` folder of the source tree the game was built from, so a worktree build uses its own shaders. Shaders, models, fonts and UI documents are loaded from there at runtime, so asset edits need no rebuild.
- **The game starts paused when the editor is enabled** (`enable_editor = true` in `app.cpp`). Scripts only tick while the editor is in Play mode — see `IScene::update`.
- `engine_logic_tests` (in `src/tests/unit_tests`) compiles the engine sources under test (render graph, shadow maths, the header-only resource atlas and ID registry, skins and the animation controller, with a stub logger; Tracy/RmlUi only as headers, so the profiling macros are empty) directly and needs neither GL nor the engine DLL. **Add new GL-free tests to it** instead of creating another executable; tests that need a GL context belong in `scenario_tests`.
- **Scenario tests** (`src/tests/scenario_tests`) run real gameplay code: the game is built as the static library `project_c_lib` (everything but `main.cpp`), which the test links. One `AppProjectC` is created for the whole run (`AppConfig` in `app.h`: hidden window, editor off so gameplay is never paused; no level is registered). A test derives from `scenario::ScenarioTest`, loads its own `IScene` with `load_scene<T>()` and advances it with `step(frames)`: every frame simulates exactly `FIXED_DT_MS` (1/60 s) whatever the real frame time, so runs are deterministic. Input: gameplay scripts read `project_c::PlayerInput` (`player_input.h`: move directions, attack, dodge, interact, drop weapon, cursor) from `AppProjectC::player_input()`, never the keyboard/mouse directly; `read_player_input_from_devices` is the only place that reads them. Whoever drives the frames sets it: the game's main loop fills it from the devices each frame (the key bindings live in `read_player_input_from_devices`), scenario tests set it with `set_input(...)`, which holds until changed. Delta times are in **milliseconds** throughout the engine and scripts. Frames still render (in a 320x180 window) but are never presented, so a frame costs well under 1 ms; most of the suite's time in Debug is loading the prefabs once (~7 s). Gameplay scenarios use `scenario::SandboxScene` (`sandbox_scene.h`: camera, light, `Encounter`, no HUD) and place only what they need with `scene->build()` (the game's `LevelBuilder`); `GameplayTest::step_aiming` points the cursor at a world position.
- There is no linter or formatter config. `-Werror` is only applied to the engine on non-MSVC compilers.
- CI (`.github/workflows/cmake-single-platform.yml`) builds Release on Windows and runs the tests labelled `ci` (`set_tests_properties(... LABELS ci)`). The runner has no GPU, so GL-dependent tests must not get the label. Release compiles out `assert`, so tests must check return values or state, not rely on an assert firing.

Android: `android-project/` is a Gradle wrapper around the root `CMakeLists.txt` (arm64-v8a); the game is built as a shared library there. `K_IS_ANDROID` in the toolkit's `utils.h` gates platform differences.

## Architecture

Three layers, each depending only on the one below:

```
src/games/project_c        executable  - gameplay: scenes, scripts, prefabs
src/engine_app_toolkit     shared lib  - C++ convenience layer: IApplication / IScene / IScript
src/engine                 shared lib  - the engine, exposed through a C API only
```

### Engine (`src/engine`)

- `include/` is the **entire public surface**: a C API in `engine.h` plus plain C structs for components in `include/components/`. Nothing in `impl/` is visible to the game or toolkit.
- `impl/engine.cpp` is the C API boundary. Opaque handles are converted to internal C++ types with the `api_cast` overloads at the top of the file; `engine_game_object_t` is an `entt::entity`.
- `engine_game_object_t` changes between runs; **files reference game objects by their persistent ID**, which is engine-internal (`Scene::get_persistent_id`; the engine's serializer translates references, so there is no public API for it). `Scene::create_new_entity` gives every entity one from a per-scene counter (`impl/entity_id_registry.h`, never reused), so building a scene in the same order gives the same IDs. Editor-only objects use `Scene::create_transient_entity` so they don't shift the IDs of the game's objects.
- The API is **context based**: `engineApplicationSetActive` / `engineSceneSetActive` store globals (`G_ACTIVE_APP`, `G_ACTIVE_SCENE`) and nearly every other call (`engineGameObject*`, `engineFrame*`, `enginePhysics*`, input) acts on the active app/scene without taking a handle. The API example in `README.md` predates this and is out of date — trust `engine.h`.
- Components are value types across the boundary: `engineGameObjectGet*Component` returns a copy and changes only take effect after `engineGameObjectUpdate*Component`. Internally that is an EnTT `replace`, and `Scene` relies on `entt::observer`s on those updates to recompute model matrices and rebuild/sync Bullet bodies.
- Adding a component type means touching: the C struct in `include/components/`, the Add/Get/Update/Remove/Has functions in `engine.h` + `engine.cpp`, any default-initialisation in `impl/components_utils/components_initializers`, the consuming system in `scene.cpp`, and (optionally) the properties panel in `application_editor.cpp`.
- `Application` owns the window/GL context, resource atlases (textures, geometries, skins, shaders, nav meshes — `named_atlas.h`, addressed by index or name; names must be unique and non-empty, so creation fails otherwise, and the names are the asset IDs scene files will use: glTF meshes, embedded images and skins are `<model file>#<name>` (`#mesh_<i>` etc. when unnamed, see `impl/asset_id.h`), e.g. `Rogue.glb#Cylinder.008`, `Rogue.glb#rogue_texture`; an image in its own file is named by its path relative to `assets/models`, so models referencing it share one texture), the `UiManager` and the `Renderer`. `ApplicationEditor` derives from it and is instantiated instead when `enable_editor` is set; it hooks the `on_*` virtuals to add the ImGui dockspace, editor camera, gizmos, entity picking, undo (`editor_commands`) and the Play/pause + game/editor view state that `engineEditorIsPlaying` / `engineEditorIsGameView` report.
- `Scene` owns the EnTT registry and a `PhysicsWorld`. `Scene::update` runs animation controllers → world transforms (`update_world_transforms`: TRS, joint attachments, parent→child) → physics (create/sync bodies, step, write dynamic bodies back) → world transforms again → skins, then gathers a `SceneRenderData` and submits it. It never calls the graphics API. World transforms are computed **before** physics so that colliders of child game objects (attack triggers, weapons) are where their parent is on the same frame, including their first frame; bodies moved by hand go through `PhysicsWorld::set_world_transform`, which also updates their broadphase bounds so ray casts see them immediately (Bullet steps at a fixed 1/60 s, so a frame can have no substep).
- Rendering is a render graph; **read `src/engine/impl/render/README.md` before changing anything under `render/`**. It covers the pass list, how to write a pass, and the `RenderFeature` extension point (post-process and overlay stages). `render_graph.h/.cpp` must stay free of GL so it remains unit-testable.
- Animation: `Skin` + `AnimationController` wrap ozz. A `Skin` is a shared, immutable **asset** in the skins atlas (skeleton + binding to the mesh; `Prefab` creates one per model, not per instance). Each instance's pose (`SkinPose`) lives in an internal component (`skin_pose_internal_component_t`) on the game object with the skin component, created in the rest pose when that component gets its skin; the animation controller on the same game object writes it. Skinned meshes and joint attachments reference that **game object** (`.skin` is an `engine_game_object_t`), not the asset, because the asset alone can't say whose pose to read. Controllers built for the same skin share an `AnimationLibrary` through an internal cache keyed by the skin asset; the library is deliberately not exposed in the C API.
- `impl/imgui` and `impl/imguizmo` are vendored copies compiled into the engine, not the submodules of the same name.
- Shaders live in `assets/shaders`; the `#version` line is added implicitly by the engine, so shader files must not declare it. `*.h` files there are shared GLSL includes.

### Toolkit (`src/engine_app_toolkit`)

Thin C++ wrappers over the C API that define the game-side object model:

- `IApplication` creates the engine application and owns a `SceneManager` (scenes are registered by type and looked up by their static `get_name()`).
- `IScene` owns one engine scene and a map of `engine_game_object_t → IScript`. One script owns one game object.
- `IScene::update` defines the frame order: flush newly registered scripts → (if playing) `update_hook_begin`, dispatch physics collisions to `on_collision`, `update`, `late_update` → `engineFrameSceneUpdate` → drop scripts whose game object was destroyed → `update_hook_end` → flush unregistrations. Script registration and unregistration are **deferred through queues**, so a script registered this frame is not in the map until the next `update`.
- `utils.h` has the `set_c_array` / `to_array` helpers for the C-array fields in component structs, and `log()`.

### Project C (`src/games/project_c`)

- `default_prefabs_data()` (`app.cpp`) maps `PrefabType` → glTF file; `AppProjectC` loads each into a `Prefab` once at startup. `Prefab` owns the engine resources parsed from the model (geometries, textures, materials, skins, animation controllers) and `instantiate()` stamps out a game-object hierarchy into a scene.
- `main.cpp` registers the level scene and owns the main loop, which holds only platform concerns: `engineFrameBegin` → read the devices into `PlayerInput` → quit (window closed or the `quit` action) → `AppProjectC::update(dt)` → `engineFrameEnd`. `AppProjectC::update` is one game frame: update the scenes, then apply scene changes they requested (`request_scene_reload<T>()`, deferred because a scene can't replace itself mid-update). Scenario tests call the same function. **Level rules live in the scene** (`TestScene::update` override: gameplay UI visibility, restart once the encounter is over), never in the loop and not in `AppProjectC`, which stays level-agnostic. There is a single level, `scenes/scene_test.cpp`: it builds the ASCII map with `LevelBuilder`, owns the nav mesh, and adds the camera, light, `Encounter` and `GameplayHud` scripts.
- **Gameplay scripts never depend on the scene class.** Win/lose rules are the `Encounter` script (`scripts/encounter_script.h`: cleared when every enemy is dead, defeated when the player is), and the HUD is the `GameplayHud` script (owns the RmlUi document `assets/ui_docs/project_c_gameplay_ui.rml` and its data bindings). Both *read* the Player/Enemy/Weapon scripts every frame; nothing reports to them. Scripts that need them use `my_scene_->find_script<Encounter>()` and must handle `nullptr`: a scene without an encounter or HUD (e.g. a scenario test scene) still plays.
- `scenes/level_builder.h` places floor tiles (each with a nav-mesh node, connected to its 8 neighbours as it is added), walls, static obstacles, the player (with its weapon), enemies, chests and weapons. The level and the scenario tests both build with it.
- Gameplay objects are scripts deriving from `BaseNode` (`scripts/base_script.h`), which is `IScript` plus a name component and prefab-aware constructors. Player and enemy logic are hand-written state machines driving animation-controller layers and animation events; scripts find each other through `my_scene_->get_script<T>(go)` (returns `nullptr` when the game object has no script of that type), or `find_script<T>()` / `get_scripts<T>()` for scripts that aren't tied to a known game object.
- `CameraScript` follows the player with a spring and shifts toward the mouse cursor (look-ahead). Occlusion is handled only by the house itself: while the player is inside, `House` hides the roof (and the props on it) and the camera facing wall.
- Player traversal and camera occlusion are documented in `src/games/project_c/CLIMBING_AND_OCCLUSION.md`. Player traversal (climb a wall, vault through a window, climb down a roof edge, mantle onto the roof at the top of a climb) starts automatically when the player walks into the obstacle; a pulsing marker shows the nearest available action whenever the player is close to a wall, window or roof edge. On the wall, WASD keeps its ground meaning: toward the wall climbs up, away climbs down. It is raycast-driven and only reacts to colliders owned by `BuildingPart` scripts (`scripts/building_script.h`). While traversing, the player's rigid body is removed so the body is moved purely by its transform; it is re-added when traversal ends. The `House` is built over the `h` cells of the ASCII map in `scene_test.cpp`; those cells get floor tiles but no nav-mesh nodes.
