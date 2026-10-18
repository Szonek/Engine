# Pixel art rendering

Low resolution rendering with hard pixels, a camera locked to the pixel grid and one pixel outlines: the "3D pixel art"
look (t3ssel8r style). The scene is rendered small and scaled up; UI and editor widgets stay at the window resolution.

Off by default. **Editor: debug panel → Pixel art**. *Apply pixel art preset* (`apply_pixel_art_preset` in
`render_settings.h`) switches everything on: 360 scene lines, camera snapping, outlines, stylized lighting with two hard
steps and colored shadows, hard shadow edges (PCF radius 0). Like the other render settings they are global, not saved
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
