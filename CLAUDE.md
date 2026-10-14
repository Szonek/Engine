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

cmake --build build --config Debug --target render_graph_tests unit_tests
ctest --test-dir build -C Debug                                # all tests
build\bin\Debug\render_graph_tests.exe --gtest_filter=Suite.Name   # single test
```

Binaries land in `build/bin/<Config>/`, libraries in `build/lib/<Config>/`.

Things that will trip you up:

- **Sources are listed explicitly** in each `CMakeLists.txt` (no globbing). A new `.cpp`/`.h` must be added to the list by hand. `src/games/project_c/scripts/solider_script.cpp` is on disk but intentionally not in the build.
- **The asset path is hardcoded** to `C:\WORK\OpenGLPlayground\assets` in `src/games/project_c/app.cpp`. Shaders, models, fonts and UI documents are loaded from there at runtime, so asset edits need no rebuild.
- **The game starts paused when the editor is enabled** (`enable_editor = true` in `app.cpp`). Scripts only tick while the editor is in Play mode — see `IScene::update`.
- `unit_tests` has almost no coverage and its app-creation test is `DISABLED_` because creating an application needs a GL context and shaders on disk. `render_graph_tests` is the meaningful suite; it compiles `render_graph.cpp` directly and needs neither GL nor the engine DLL.
- There is no linter or formatter config. `-Werror` is only applied to the engine on non-MSVC compilers.
- CI (`.github/workflows/cmake-single-platform.yml`) only builds Release on Windows; it does not run tests.

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
- The API is **context based**: `engineApplicationSetActive` / `engineSceneSetActive` store globals (`G_ACTIVE_APP`, `G_ACTIVE_SCENE`) and nearly every other call (`engineGameObject*`, `engineFrame*`, `enginePhysics*`, input) acts on the active app/scene without taking a handle. The API example in `README.md` predates this and is out of date — trust `engine.h`.
- Components are value types across the boundary: `engineGameObjectGet*Component` returns a copy and changes only take effect after `engineGameObjectUpdate*Component`. Internally that is an EnTT `replace`, and `Scene` relies on `entt::observer`s on those updates to recompute model matrices and rebuild/sync Bullet bodies.
- Adding a component type means touching: the C struct in `include/components/`, the Add/Get/Update/Remove/Has functions in `engine.h` + `engine.cpp`, any default-initialisation in `impl/components_utils/components_initializers`, the consuming system in `scene.cpp`, and (optionally) the properties panel in `application_editor.cpp`.
- `Application` owns the window/GL context, resource atlases (textures, geometries, shaders, nav meshes — `named_atlas.h`, addressed by index or name), the `UiManager` and the `Renderer`. `ApplicationEditor` derives from it and is instantiated instead when `enable_editor` is set; it hooks the `on_*` virtuals to add the ImGui dockspace, editor camera, gizmos, entity picking, undo (`editor_commands`) and the Play/pause + game/editor view state that `engineEditorIsPlaying` / `engineEditorIsGameView` report.
- `Scene` owns the EnTT registry and a `PhysicsWorld`. `Scene::update` runs physics, transforms, animation controllers, joint attachments, skins and parent→child transforms, then gathers a `SceneRenderData` and submits it. It never calls the graphics API.
- Rendering is a render graph; **read `src/engine/impl/render/README.md` before changing anything under `render/`**. It covers the pass list, how to write a pass, and the `RenderFeature` extension point (post-process and overlay stages). `render_graph.h/.cpp` must stay free of GL so it remains unit-testable.
- Animation: `Skin` + `AnimationController` wrap ozz. Controllers built for the same skin share an `AnimationLibrary` through an internal cache; the library is deliberately not exposed in the C API.
- `impl/imgui` and `impl/imguizmo` are vendored copies compiled into the engine, not the submodules of the same name.
- Shaders live in `assets/shaders`; the `#version` line is added implicitly by the engine, so shader files must not declare it. `*.h` files there are shared GLSL includes.

### Toolkit (`src/engine_app_toolkit`)

Thin C++ wrappers over the C API that define the game-side object model:

- `IApplication` creates the engine application and owns a `SceneManager` (scenes are registered by type and looked up by their static `get_name()`).
- `IScene` owns one engine scene and a map of `engine_game_object_t → IScript`. One script owns one game object.
- `IScene::update` defines the frame order: flush newly registered scripts → (if playing) `update_hook_begin`, dispatch physics collisions to `on_collision`, `update`, `late_update` → `engineFrameSceneUpdate` → drop scripts whose game object was destroyed → `update_hook_end` → flush unregistrations. Script registration and unregistration are **deferred through queues**, so a script registered this frame is not in the map until the next `update`.
- `utils.h` has the `set_c_array` / `to_array` helpers for the C-array fields in component structs, and `log()`.

### Project C (`src/games/project_c`)

- `main.cpp` maps `PrefabType` → glTF file; `AppProjectC` loads each into a `Prefab` once at startup. `Prefab` owns the engine resources parsed from the model (geometries, textures, materials, skins, animation controllers) and `instantiate()` stamps out a game-object hierarchy into a scene.
- `AppProjectC::run` is the main loop (`engineFrameBegin` → `update_scenes` → `engineFrameEnd`). There is a single scene, `scenes/scene_test.cpp`; it builds the level, owns the nav mesh and the RmlUi data bindings for the gameplay HUD (`assets/ui_docs/project_c_gameplay_ui.rml`), and tracks encounter state.
- Gameplay objects are scripts deriving from `BaseNode` (`scripts/base_script.h`), which is `IScript` plus a name component and prefab-aware constructors. Player and enemy logic are hand-written state machines driving animation-controller layers and animation events; scripts find each other through `my_scene_->get_script<T>(go)` (returns `nullptr` when the game object has no script of that type).
- Player traversal (E key: climb a wall, vault through a window, climb down a roof edge; mantle onto the roof at the top of a climb) is raycast-driven and only reacts to colliders owned by `BuildingPart` scripts (`scripts/building_script.h`). While traversing, the player's rigid body is removed so the body is moved purely by its transform; it is re-added when traversal ends. The `House` is built over the `h` cells of the ASCII map in `scene_test.cpp`; those cells get floor tiles but no nav-mesh nodes.
