# REVIEW.md

Rules for reviewing changes to this repository. They come on top of `CLAUDE.md`, which describes the current
architecture. A violation of a rule in a **Blocking** section is a real finding, not a nit, even when the code works.

## Blocking: design

### 1. Every new engine API needs a justification

The C API in `src/engine/include/` (`engine.h` and the component structs) is the engine's entire public surface.
Every addition to it, or change of it, must be justified in the PR description:

- **Why the game needs it**: the concrete caller (a script, the editor, a test) and what it can't do today.
- **Why it can't be done with what exists**: existing functions, components or toolkit helpers.
  An API that a few lines in the toolkit or the game could replace is rejected.
- **Why it belongs in the engine**, and not in `engine_app_toolkit` (C++ convenience) or the game.
- **Its shape**: it follows the existing conventions (context based, acts on the active app/scene; game objects
  passed as `engine_game_object_t`; components are values, get/update; failures are return values /
  `ENGINE_INVALID_OBJECT_HANDLE`, never asserts; no internal pointers leave the engine).

The same applies to new public component types and new fields in existing ones: a field must be authored data,
never derived data (that goes into an engine-internal component).
Removing or renaming API needs no justification beyond the PR's purpose, but every caller is updated in the same PR.

### 2. Every feature matches an industry-standard design

New features follow how established engines (Unity, Unreal, Godot, Bevy) or established libraries (Jolt,
Recast/Detour, ozz-animation, EnTT/Flecs) solve the same problem, and the PR description names the reference,
e.g. "like Unity's `CharacterController` / Godot's `move_and_slide`" or "lifecycle like Unity's `Awake`/`Start`".
Code comments and `CLAUDE.md` do the same where the design is explained.

- Deviations from the reference are deliberate and explained: what differs and why it's better here.
- A home-grown design without a reference is rejected **unless** the PR states explicitly that the feature is
  unique to this engine/game and why no standard design fits.
- Naming follows the reference where reasonable, so the concept is recognisable (body types dynamic/kinematic/static,
  nav mesh modifier, prefab, transient object).

### 3. Architecture changes are deliberate

`CLAUDE.md` describes the current architecture, and it is expected to evolve.
A change that departs from it (layering, ownership, where a system lives, a convention) is fine when it is deliberate:
the PR description says what changes and why, and `CLAUDE.md` is updated in the same PR.
A departure that isn't called out is a finding.

## Blocking: tests

- **Every new feature and every bug fix comes with tests.** A bug fix's test fails without the fix.
  A PR without them explains why testing is impractical.
- New engine logic that can run without GL is tested in `engine_logic_tests`, not a new executable.
  GL-dependent behaviour is tested in `scenario_tests`.
- Engine tests use the generated models in `assets/models/test_fixtures` (added through `generate_test_models.py`),
  never the game's models.
- Gameplay scenario tests assert outcomes, not model details, and drive input with `set_input`, never devices.
- Tests check return values or state, never rely on an `assert` firing (Release compiles them out).
- Only tests that run without a GPU get the `ci` label.

## Blocking: documentation

- A change to the architecture, ownership, a convention or a "things that will trip you up" fact updates `CLAUDE.md`
  in the same PR.
- Render changes update `src/engine/impl/render/README.md`; traversal and occlusion changes update
  `src/games/project_c/CLIMBING_AND_OCCLUSION.md`.
- `README.md`'s API example is known to be out of date: don't flag it, don't copy from it.

## Common mistakes to check for

Not rules: mechanics of the current code that are easy to get wrong. Report a mistake as a bug when it causes one.

- **Build**: every new `.cpp`/`.h` is listed in its `CMakeLists.txt` (no globbing). Shaders have no `#version` line.
- **Components across the C API are copies**: a `Get*Component` without the matching `Update*Component` is a lost change.
- **Assets**: handles are slot + generation and components store them without references. Whoever loads an asset
  holds it and releases it (prefabs before their application is destroyed); asset IDs follow `impl/asset_id.h`.
- **Hierarchy**: any hierarchy change invalidates child indices, so children are copied before destroying or
  reparenting them.
- **Persistent IDs**: objects spawned at runtime are transient, so they never shift the IDs of level content or get saved.
- **Script lifecycle**: constructors set up only their own game object; anything involving other objects goes into
  `on_start()`. `find_script` / `get_script` results are null-checked.
- **Input**: gameplay reads `PlayerInput`, never the keyboard or mouse.
- **Units**: delta times are in milliseconds; collider and capsule sizes are in the game object's scaled space.
- **Scene files**: a new authored component is described in `builtin_types.cpp` at the right place in the load order,
  and saving stays deterministic (the same scene gives the same bytes).

## Non-blocking (report as nits)

- Code doesn't read like its surroundings (naming, comment density, idioms).
- Unnecessary per-frame allocations or work in hot paths (scene update, render data gathering, script updates).
- Missing Tracy zones on new per-frame systems.
- Missed simplifications, dead code, duplication of an existing helper (`project_c::utils`, the toolkit's `utils.h`).

## Out of scope

- Third-party code under `thirdparty/` and the vendored `impl/imgui`, `impl/imguizmo`.
- Formatting (there is no formatter config).
- Non-Windows desktop build issues, except ones that break Android (`K_IS_ANDROID`).
