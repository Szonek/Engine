# Terrains

A terrain is the ground of an outdoor area: a grid of heights (like Unity's Terrain / Unreal's Landscape), not a mesh
modelled in Blender and not floor tiles. The engine draws it, collides with it and builds the nav mesh from it.

## Files

| File | What |
|---|---|
| `*.terrain` | A terrain asset: the heights, in metres. Binary (`TerrainData::serialize`): `ENGTERR\0`, version, samples along x and z, cell size, then one float per sample. Its asset ID is its path here (`outdoor_test.terrain`), which is what scene files reference. |
| `*.png`, `*.r16`, `*.raw` | A heightmap, the source a terrain is imported from: a 16-bit grayscale PNG, or a square raw 16-bit little endian file without a header. Gaea, World Machine and Blender export both. 8-bit images are refused (256 heights give terraces). |
| `generate_test_terrains.py` | Generates the files here (the tests' fixture and the outdoor test level's terrain). |

## Making one

* **From a heightmap**: put it in this folder, select it in the editor's asset browser, set the cell size (metres
  between two pixels) and the heights of black and white, and press Import. It writes `<name>.terrain` next to it; a
  scene showing that terrain shows the new heights at once. From code: `engineTerrainImportHeightmap`, then
  `engineTerrainSave`.
* **From code** (a generator): `engineTerrainCreate` with the heights, or flat and then `engineTerrainSetHeights` for
  the regions to shape. Name it like a file (`generated/zone.terrain`) and call `engineTerrainSave` to keep it;
  otherwise create it again before loading a scene which references it.

## Using one

Drag the `.terrain` file from the asset browser into the scene view. That creates a game object with:

* a **terrain** component (the asset, and how many metres one repeat of the textures covers),
* a **material** component, which it is drawn with, like a mesh,
* a **collider** of type heightfield referencing the same asset: characters walk on it, rays hit it, and
  `engineNavMeshBuild` reads it. Put it on the level geometry layer in a level of the game.

The terrain starts at its game object's origin and extends along +x and +z. One sample per metre is plenty for the
game's camera; a side of 2^n + 1 samples (65, 129, 257, ...) gives whole chunks of 32 cells, the unit it is culled by.

## Sculpting

Select the terrain's game object in the editor (Edit mode, Editor view). Under its components, **Terrain tools** has
the brushes:

| Brush | What it does |
|---|---|
| Raise / Lower | Lifts or digs the ground under the brush; Shift does the other one. |
| Smooth | Evens out bumps and steps. |
| Flatten | Brings the ground to the height under the cursor where the stroke starts: plateaus, paths, ground for buildings. |

Hold the left mouse button over the terrain. Radius (`[` and `]` change it), strength and falloff are in the panel;
Escape turns the tool off, which gives the scene view back to selecting and moving game objects. A stroke is one undo
step. The terrain shows `*` until it is saved: its Save terrain button, or Ctrl+S with the scene. Build the nav mesh
again (its button under the Nav Surface) after changing ground characters walk on.

Limits for now: the nav mesh is built in one piece, so keep terrains with a nav surface small (the outdoor test level
is 64 m; a tiled nav mesh is step 6 of the big scenes epic); no holes; one material for the whole terrain (painted
layers are step 4b-2).
