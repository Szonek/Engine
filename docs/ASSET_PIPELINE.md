# Low-poly asset pipeline

How to make our own low-poly assets in Blender instead of relying on asset kits, what the engine can load today,
and the engine/game changes planned to make the Blender → game loop fast for a one-person project.

The guiding idea: keep the art workflow as simple as the KayKit / Synty kits we use now (they are the right model
for a stylised solo project), stop bending assets around loader quirks, and invest in pipeline tooling rather than
renderer features.

| File                                         | What lives there                                                        |
|----------------------------------------------|-------------------------------------------------------------------------|
| `src/engine/impl/gltf_parser.cpp`            | glTF loading: vertex attributes, materials, skins, animations           |
| `src/engine/impl/material.h/.cpp`            | Engine material (diffuse colour + diffuse texture)                      |
| `assets/shaders/lit.fs`                      | Lit shading; only `texture_diffuse * diffuse_color` (+ specular) is used |
| `src/games/project_c/prefab_types.h`         | `PrefabType` enum, one entry per model                                  |
| `src/games/project_c/app.cpp`                | `default_prefabs_data()`: `PrefabType` → glTF file                      |
| `src/games/project_c/prefab.h/.cpp`          | `Prefab`: engine resources parsed from a model, `instantiate()`         |
| `src/games/project_c/scenes/level_builder.*` | Places tiles, walls, props; colliders are set up here in C++            |

## The pipeline at a glance

Industry steps, and what we actually do of each:

| Step                    | AAA                                         | This project                                               |
|-------------------------|---------------------------------------------|------------------------------------------------------------|
| Reference + blockout    | Concept art, greybox                        | Blockout with primitives at real scale, test in game early |
| Modelling               | Sculpt high-poly, retopologise, bake        | Model the final low-poly mesh directly                     |
| UVs                     | Careful unwrap, texel density               | Drop faces onto palette swatches (no real unwrap)          |
| Texturing               | Substance, full PBR map set                 | **One shared palette PNG, base colour only**               |
| Rigging                 | Per-character rigs, retargeting             | **One shared humanoid skeleton for all characters**        |
| Animation               | Mocap + cleanup                             | Hand-keyed Actions, one Action per clip                    |
| Export                  | FBX / proprietary, cooked                   | glTF 2.0, one asset per file, via an export script         |
| Integration             | Asset database, cooking, LODs               | Drop a file in `assets/`, hot-reload, gameplay data from the `.blend` |
| Versioning              | Perforce                                    | `.blend` under Git LFS, exports committed as plain files   |

Deliberately skipped: PBR / normal maps / high-to-low baking, Substance, FBX, LODs, an asset database or cook step.
At our art style, asset counts and poly budgets they cost more than they give. The look comes from lighting (shadows,
SSAO, and possibly stylised shading and outlines later) instead. The existing Sobel pass (`outline_feature`) is only
the editor's selection outline, not a game outline.

## Art conventions

### Palette texture

All props, tiles and characters share one small palette texture (64×64 or 128×128 PNG of colour swatches). Each
face's UVs are scaled down to a point and dropped on a swatch (Smart UV Project, then scale the island to 0 and move
it). This is exactly what the KayKit models do (`rogue_texture.png`, `prototypebits_texture.png`).

Why: almost no UV work, a coherent colour scheme for free, one shared texture, and the whole game can be recoloured
by editing one PNG. Keep one material per object that references the palette.

### Scale, orientation, origins

- 1 Blender unit = 1 metre. Snap to a grid that matches the tile size from day one.
- Apply all transforms (Ctrl+A → All Transforms) before export. Export with "+Y Up" (the exporter default).
- Origins where gameplay expects them: feet at (0,0,0) for characters, the base for props, the same corner/centre
  `LevelBuilder` assumes for the KayKit tiles and walls for modular pieces.

### Characters

- **One skeleton for every humanoid.** Animations are made once and shared; the engine already shares an
  `AnimationLibrary` between controllers built for the same skin. Start from the KayKit rig (import a KayKit
  character into Blender) so existing animations and code keep working, or make our own and stick to it.
- Names the gameplay code currently relies on:
  - bones: `handslot.r`, `handslot.l` (weapon attachment), `hips`, `spine` (upper-body attack mask);
  - clips: `Unarmed_Idle`, `Running_A`, `Running_Strafe_Left/Right`, `Death_A`, `Jump_Idle`,
    `Jump_Full_Short`, `Dualwield_Melee_Attack_*`, ...
  The planned sockets (below) replace the hard-coded attachment bones.
- At most 4 bone influences per vertex (exporter: Data → Skinning → Influence Limit 4); only `JOINTS_0` /
  `WEIGHTS_0` are read.
- One Action per clip, named cleanly; export all actions.

### Export settings (glTF 2.0)

- `.glb`, or `.gltf` + `.bin` + texture. When several models share the palette PNG, use separate `.gltf` files that
  all reference the same PNG: textures loaded from a file are named by their URI, so they are loaded once.
- Include → Selected Objects only; one asset per file (one file per prefab).
- Apply Modifiers on, Materials: Export, no cameras or lights.
- Animation: keep "Sampling" on, which bakes to LINEAR keys (the only interpolation the loader supports).

These settings are what the export script (step 4 below) fixes in place, so they never drift.

### Folder layout

```
art/                       # Blender sources, Git LFS
  palette.png
  props/crate.blend
  characters/hero.blend
assets/models/project_c/   # exported glTF, what the game loads
  palette.png
  crate.gltf
  hero.glb
```

Assets are read from `assets/` at runtime (`PROJECT_C_ASSETS_DIR`), so a re-export needs no rebuild.

## What the loader supports today

Limits in `gltf_parser.cpp` / `lit.fs` an asset must respect until the changes below land:

| Feature                          | Status                                                                           |
|----------------------------------|----------------------------------------------------------------------------------|
| Vertex attributes                | `POSITION`, `NORMAL`, `TEXCOORD_0`, `JOINTS_0`, `WEIGHTS_0`; anything else (e.g. `COLOR_0`) ignored |
| Materials                        | `baseColorFactor` + `baseColorTexture` only; normal / metallic-roughness / emissive ignored |
| Multiple materials on one mesh   | **Asserts** ("Currently not supporting multi-material meshes!")                  |
| Transparency                     | **Asserts** if `baseColorFactor` alpha != 1                                      |
| Animation interpolation          | LINEAR only, asserts otherwise                                                   |
| Colliders, attach points, tags   | Not read from the file; set up in C++ (`LevelBuilder`, scripts)                  |
| `extras` / custom properties     | Ignored                                                                          |
| Hot-reload                       | Shaders and RmlUi documents only, not models                                     |
| Adding a model                   | New `PrefabType` entry + line in `default_prefabs_data()` + rebuild              |

## Planned changes

In order. Each is an independent change.

### 1. Loader fixes

Small, and they remove every case where an asset has to be shaped around the loader:

- **Multi-material meshes** - one draw per primitive instead of the assert. A stray second material slot just works.
- **Emissive colour** (`emissiveFactor`, optionally `emissiveTexture`) - glowing pickups, runes, lava.
- **Alpha cutout** (`alphaMode: MASK` + `alphaCutoff`, alpha test only, no sorting) - foliage, fences, hair cards.
- **Vertex colours** (`COLOR_0`), optional - gradients and baked AO on top of the palette.

Not worth doing: STEP / CUBICSPLINE interpolation (the exporter samples to LINEAR anyway), PBR maps.

### 2. Data-driven prefabs

Replace the `PrefabType` enum + `default_prefabs_data()` table with a manifest (e.g. `assets/prefabs.json`) or a
scan of a models folder: dropping a `.glb` in makes it available. Gameplay refers to prefabs by name. Bone and clip
names a script needs move to per-character data, so a new character is not tied to KayKit names in C++.

### 3. Model hot-reload

Register glTF files with `FileWatcher` (it already reloads shaders and UI documents) and re-instantiate prefab
instances on change. Re-export from Blender, see the result in the running game.

### 4. One-click Blender export

A small Python add-on/script (kept in the repo, e.g. `tools/blender/`) that exports each asset collection to
`assets/models/project_c/` with the fixed settings above. Removes "it looks wrong in game" caused by export
settings drifting between exports.

### 5. Gameplay data authored in Blender

The biggest change; Unreal (`UCX_`, `SOCKET_`) and Godot (`-col`, `-noimp`) both do this:

| In Blender                                   | In the engine                                                     |
|----------------------------------------------|-------------------------------------------------------------------|
| Mesh named `COL_box_*` / `COL_hull_*` / ...  | Bullet collider of that shape, not rendered                       |
| Empty named `SOCKET_<name>`                  | Attach point (`handslot.r`, `muzzle`, ...) replacing hard-coded joint names and offsets |
| Custom property on an object (→ glTF `extras`) | Tags / parameters on the game object, e.g. `climbable: true`    |

Colliders and attach points are then tuned visually instead of as numbers in C++. Needs a short design pass first:
the naming has to fit the existing traversal raycasts, which only react to colliders owned by `BuildingPart`
scripts (see `src/games/project_c/CLIMBING_AND_OCCLUSION.md`), and the C API needs a way to expose parsed
`extras` to the game.

### Later, maybe

- **Blender as a level editor**: one glTF scene with empties for spawns and markers. Only once a modular kit exists;
  the ASCII map + `LevelBuilder` fits the grid-based traversal and nav mesh for now.

## Learning order

1. A static prop (crate, barrel) on the palette; swap it in for `Box_B.gltf` and check scale and lighting.
2. A modular wall / floor tile that lines up with `LevelBuilder`'s grid.
3. A weapon: origin and orientation matter because it attaches to a hand bone (later a socket).
4. A character skinned to the shared skeleton, reusing the KayKit animations.
5. Own animations - the most time-consuming part, so last.
