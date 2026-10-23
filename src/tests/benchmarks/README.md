# Benchmarks

Programs that run real game code (`project_c_lib`) in a visible window with vsync off and report frame times.
They need a GPU, so they are not tests: neither CTest nor CI runs them. Run them from a Release build.

## big_scene_benchmark

Builds dungeon-like scenes of 32x32, 64x64 and 128x128 cells with the game's `LevelBuilder`, the way the level is built
today: a floor tile per cell, wall pieces around 8x8 rooms with doors, two chests, a point light and an enemy per room,
the player and the `Encounter` rules. Each size is measured with the gameplay camera and with the whole map in view.

```powershell
cmake --build build --config Release --target big_scene_benchmark
build\bin\Release\big_scene_benchmark.exe --out results.md
build\bin\Release\big_scene_benchmark.exe --sizes 64 --enemies 0      # one size, without enemies
```

Options: `--frames N` measured frames per shot (600), `--warmup N` frames before measuring (120), `--sizes 32,64,128`,
`--enemies 0|1`, `--out <file>` writes the report as Markdown. Update is `AppProjectC::update` (scripts, animation,
physics, world transforms, gathering draw items); frame end is `engineFrameEnd` (rendering, present, waiting for the GPU).

Tracy has to be built on demand (`TRACY_ON_DEMAND`), otherwise it keeps every zone in memory and the 128x128 scene runs
out of it.

### Baseline (2026-10-23)

Release, RTX 3060 Laptop GPU, 1920x1080, median frame time in ms.

| Cells | Meshes | Colliders | Enemies | Update | Frame end | Whole frame | FPS |
|---|---|---|---|---|---|---|---|
| 32x32 | 1,346 | 1,329 | 15 | 1.2 | 10.5 | 11.7 | 85 |
| 64x64 | 5,250 | 5,185 | 63 | 7.4 | 39.6 | 47.1 | 21 |
| 128x128 | 20,738 | 20,481 | 255 | 66.6 | 154.3 | 221.3 | 5 |

The gameplay view and the whole-map view cost the same (nothing is culled). Without enemies the update is 0.4, 1.7 and
8.5 ms. Nav mesh build: 36, 150 and 646 ms.
