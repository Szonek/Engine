# Pixel art rendering

Low resolution rendering with hard pixels, a camera locked to the pixel grid and one pixel outlines: the "3D pixel art"
look (t3ssel8r style). The scene is rendered small and scaled up; UI and editor widgets stay at the window resolution.

Off by default. **Editor: debug panel → Pixel art**. *Apply pixel art preset* (`apply_pixel_art_preset` in
`render_settings.h`) switches everything on: 360 scene lines, camera snapping, outlines, stylized lighting with two hard
steps and colored shadows, crisp shadow edges (4096 shadow map over 35 units, PCF 1, `hard_edges`). Like the other render settings they are global, not saved
and not in the C API.

| File                                   | What lives there                                                                |
|----------------------------------------|---------------------------------------------------------------------------------|
| `render_settings.h`                    | `PixelArtSettings`, `apply_pixel_art_preset`                                    |
| `pixel_art.h/.cpp`                     | Scene target size, low resolution camera, camera snapping. **No GL**, unit tested in `pixel_art_tests.cpp` |
| `renderer.cpp`                         | uses them: cameras in `prepare_gpu_data`, target size in `render_frame`, Present, picking (`read_entity_id`) |
| `pixel_outline_pass.h/.cpp`            | the "Pixel Outline" pass                                                        |
| `assets/shaders/present.fs`            | scales the scene up: picks texels without filtering                             |
| `assets/shaders/pixel_outline.fs`      | silhouettes and creases                                                         |
| `assets/shaders/lit.fs`                | writes view space normals (`out_normal`, third color target) for the outline    |

## 1. Low resolution scene targets

`target_height` is the height of the scene in pixels. The pixel scale N is the whole number closest to
`window height / target_height` (`compute_pixel_scale`): 360 gives 3 at 1080p, 4 at 1440p and 6 at 2160p, so every
screen shows the same 360 lines, and every scene pixel is an equal NxN block (a fractional scale would make some pixel
rows and columns wider than others). Windows which aren't a multiple get slightly more or fewer lines.

With pixel scale N the scene targets (`SceneColor`, `EntityId`, `SceneDepth`, `SceneNormals`) are `window / N`
(rounded up) plus a margin of `PIXEL_ART_MARGIN` texels on every side. All scene passes (shadow map, depth pre-pass, SSAO,
Forward, ...) and post-process features run at that size: `FrameContext::width/height` is the size of the scene targets,
not of the window. Present writes the window sized backbuffer and every window pixel takes the texel under it, so a
texel becomes an NxN block of window pixels.

The camera the game set up is made for the window. The renderer renders with a copy whose projection is shrunk in NDC by
`(window / N) / target size`: the window's view lands in the middle of the targets and the margin around it shows a bit
more of the world. Present maps window uv to target uv with `PresentTransform` (`uv * scale + offset`).

## 2. Camera snapping

When a camera moves by a fraction of a texel, every edge in the image is rasterized a little differently: edges crawl
and single pixels flicker. Pixel art must not do that. So an orthographic camera is rendered from the nearest position at
which the world falls on the texel grid exactly like it did before: the translation of the view matrix (world origin in
view space) is rounded to whole texels. Every world point then always lands at the same place inside its texel, and
moving the camera only shifts the whole image by whole texels.

The part of the movement which was rounded away (less than half a texel) is applied by Present: it shifts where the
window samples the targets, so the image on the screen moves by whole window pixels, smoothly, and is where the camera
really is. The margin is what becomes visible at the window edges during that shift.

Only the image is moved, not the game's camera: gameplay (mouse cursor projection, ImGuizmo, ...) keeps using the
camera it set up and stays exactly aligned with what is on the screen. Picking maps window pixels through the same
`PresentTransform` into the entity id target (`Renderer::read_entity_id`).

Limits:
- Snapping assumes the camera only moves, it doesn't rotate (the game camera keeps its pitch). A rotating camera can't
  have a stable texel grid.
- Perspective cameras (the editor camera) are rendered at low resolution without snapping.
- All cameras draw into the same targets, Present uses the snapping of the first one.

### Moving objects

The camera snap keeps everything that stands still stable. An object that moves through the world (the player, enemies,
anything the camera follows) is at a different fraction of a texel every frame, so its edges are rasterized differently
every frame and flicker. Objects are therefore snapped too (`compute_object_snap_offset`, `Renderer::snap_objects_to_pixel_grid`):
every mesh is moved so that its **snap origin** lands on the texel grid (less than half a texel on the screen). The
whole mesh moves by the same offset, so it keeps its exact shape and only moves in whole texels.

The offset is horizontal (world XZ), the object keeps its height. Moving it along the camera's right and up instead
would also move it up and down in the world (the camera looks down at 45 degrees), so a walking character would bob by a
fraction of a texel every frame and its shadow on the ground would jitter. At a constant height, the shadow on flat ground
is at a constant offset from the object, so it lands on the texel grid too and moves with it (unit tested in
`ShadowOfASnappedObjectKeepsItsPlaceInTheTexel`). Only a camera looking horizontally falls back to the camera plane.

The snap origin is the world position of the topmost ancestor of the mesh (`hierarchy_root_position` in
`Scene::submit_render_data`): all parts of a prefab and everything attached to it (a weapon in the hand) share it, so they
are snapped together and never separate. Consequences:
- Don't parent moving objects under a static grouping node (like the editor's "enviorment" group): they'd be snapped by
  the node's position, i.e. not at all, and flicker again.
- Within one hierarchy, parts moving relative to the root (animated bones, a swinging weapon) aren't snapped; that is
  animation, the shape really changes.
- Only meshes are snapped, not sprites or debug lines.

What snapping can't remove: an animated mesh changes shape every frame, and at this resolution even a small change
(idle breathing) moves some edge pixels. That is the "boiling" of animated low resolution 3D. The pixel art answer is to
animate at a lower frame rate (stepped animation, ~12 fps), like sprite animation; it isn't done by the engine yet.

### Shadows

A shadow map texel close to the size of a scene pixel aliases with the pixel grid: shadow edges become ragged staircases
of irregular steps, and shift whenever a caster moves relative to the shadow map. The preset makes shadow map texels
several times smaller than a scene pixel (4096 map; with `distance` 35 one texel is ~0.01 world units, a scene pixel is
~0.03) and turns on `ShadowSettings::hard_edges`: the PCF filtered shadow is thresholded at one half, so the edge is crisp
but placed with sub-texel precision. Every scene pixel then takes the shadow at its own center, and edges follow the
scene pixel grid like everything else.

Measured offscreen (walking character, camera following it, 1080p, game camera): shadow pixels that change shape from one
frame to the next went from 2.1% (shadows of 2048 over 40 units, PCF 0, objects snapped in the camera plane) to 0.26%.

What remains: the shadow map texel grid is fixed in the world, a moving caster is rasterized into it a little
differently every frame, so a rare edge pixel still changes. Self-shadowing on a moving character changes the same way.

## 3. Outlines ("Pixel Outline" pass)

After the scene passes and before the post-process features (the editor's selection outline and grid are not
outlined). It reads scene color, depth and normals (written by the Forward pass only when the outline is enabled) and
writes a new scene color. For every pixel and its 4 neighbours:

| Edge       | Condition                                                                                         | Result |
|------------|---------------------------------------------------------------------------------------------------|--------|
| silhouette | a neighbour is further away than `outline_depth_threshold` (or empty): this pixel is the edge of the nearer object | darkened by `outline_strength` |
| crease     | not a silhouette, a neighbour's normal differs by more than `highlight_normal_threshold` (1 - cos) and the neighbour is not nearer | brightened by `highlight_strength` |

Both mark only the pixel on the near side of the edge, so lines are one scene pixel wide. Depth is converted to view
space distance with the inverse projection of the first camera, thresholds are in world units for both orthographic
and perspective cameras.

## Tuning notes

- What decides the look is how many scene pixels one world unit covers: target height / world units visible vertically.
  With 360 lines and the game camera (`ORTHO_HALF_HEIGHT` 5, 10 units visible) it's 36 px per unit. Art guidelines for
  that density are in `docs/ASSET_PIPELINE.md` (Designing for the pixel art look).
- Art for this look wants simple shapes and few colors; detailed textures turn into noise at low resolution.
- Smooth-normal meshes (characters) get few creases, which is usually right; lower `highlight_normal_threshold` for more.
