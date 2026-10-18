# Shadows and SSAO

Both effects answer the same question for every pixel: **how much light actually gets here?**

* A **shadow** answers it for one light: is something standing between this point and the directional light?
* **Ambient occlusion** (AO) answers it for the light that comes from everywhere (the `ambient` term): is this point
  in a corner, a crease or under something, where less of the surrounding light can reach it?

The lit shader then scales the two parts of its lighting by the two answers:

```
color = ambient * ambient_occlusion  +  (diffuse + specular) * shadow
```

Neither answer can be found while shading a single triangle, because both depend on *other* geometry. So both are
prepared by earlier passes which render the depth of the whole scene, and the lit shader only looks the result up.

## Frame overview

Passes of one camera, in order (all of them are visible in the editor's debug panel and in RenderDoc):

| Pass             | Draws                         | Writes                    | Shader                         |
|------------------|-------------------------------|---------------------------|--------------------------------|
| Shadow Map       | all meshes, seen from the light  | `ShadowMap` (depth)    | `depth_only.fs`                |
| Depth Pre-pass   | all meshes, seen from the camera | `SceneDepth`           | `depth_only.fs`                |
| SSAO             | full screen quad              | `AmbientOcclusionNoisy`   | `ssao_common.h` + `ssao.fs`    |
| SSAO Blur        | full screen quad              | `AmbientOcclusion`        | `ssao_common.h` + `ssao_blur.fs` |
| Forward          | all meshes, seen from the camera | `SceneColor`, `EntityId` | `shadow_helpers.h` + `lit.fs` |

Where things live:

| File                                   | What                                                                 |
|----------------------------------------|----------------------------------------------------------------------|
| `render_settings.h`                    | all tweakable values (`ShadowSettings`, `SsaoSettings`), with defaults |
| `shadows.h/.cpp`                       | math which places the shadow map in the world. No GL, unit tested    |
| `ssao_pass.h/.cpp`                     | the two SSAO passes, sample kernel and noise texture                 |
| `renderer.cpp`                         | declares the passes (`add_scene_passes`), uploads per camera data (`prepare_gpu_data`) |
| `material.h/.cpp`                      | `Material*DepthOnly` used by Shadow Map and Depth Pre-pass           |
| `assets/shaders/shadow_helpers.h`      | GLSL: shadow lookup and AO lookup used by `lit.fs`                   |

Everything can be changed live in the editor: **debug panel → Shadows / SSAO**. `Debug view` there shows only the
ambient occlusion or only the shadow term on the geometry, which is the fastest way to understand what a setting does.
Shader files are watched and recompiled on save, but `FileWatcher` keeps one callback per file: a file shared by
several shader programs (`lit.fs`, `shadow_helpers.h`, `depth_only.fs`, ...) reloads only the first of them, in practice
the static mesh variant. `ssao.fs` and `ssao_blur.fs` have a single user and reload fine; for the others restart the game.

---

## Shadows (shadow mapping)

### The idea

A point is in shadow if the light can't see it. So the scene is rendered once more **from the light's point of view**,
keeping only depth. The result, the *shadow map*, stores for every direction from the light the distance to the
closest surface, i.e. the surface which *is* lit.

Later, when the lit shader shades a pixel, it asks: "how far from the light am I, and how far is the closest surface
the light saw in my direction?"

```
my distance  >  stored distance   ->  something is in front of me  ->  shadow
my distance  <= stored distance   ->  I am that closest surface    ->  lit
```

That is the whole algorithm. Everything else below deals with its two weaknesses: the shadow map has a limited
resolution, and depth has a limited precision.

Only the **first directional light** of a scene casts shadows. A directional light (sun) has no position, only a
direction, so its "camera" uses an orthographic projection: all light rays are parallel.

### Step 1: where to put the shadow map (`shadows.cpp`)

The shadow map has a fixed number of texels (2048 x 2048 by default). The smaller the area of the world it covers, the
sharper the shadows. So it covers only what the camera can see, and only up to `ShadowSettings::distance`:

1. Take the 8 corners of the camera frustum, with the far plane pulled in to the shadow distance.
2. Wrap them in a bounding **sphere**. A tight box would be smaller, but its size changes when the camera rotates and
   shadow resolution would change with it. A sphere is the same from every angle.
3. Build the light's view matrix (looking along the light direction) and an orthographic projection which encloses
   the sphere.
4. Move the sphere center only in steps of **one shadow map texel**. Otherwise the texel grid slides over the world
   as the camera moves, each frame samples the casters slightly differently and shadow edges shimmer.
5. Push the near plane towards the light by `caster_distance`. A tall object outside of the visible area can still
   throw its shadow into it, so it has to be inside of the shadow map too.

The result is a view and a projection matrix: the light is treated as just another camera. That's why the Shadow Map
pass can reuse the normal vertex shaders. It gets a different `CameraData` uniform buffer and nothing else changes.

### Step 2: render the shadow map

`Renderer::draw_meshes_depth_only` draws every static and skinned mesh with `depth_only.fs`, an empty fragment
shader: the rasterizer writes depth on its own, and there is no color target at all.

### Step 3: the lookup (`calc_directional_shadow` in `shadow_helpers.h`)

For a pixel at `world_pos`:

```glsl
vec3 shadow_coords = (shadow_view_projection * vec4(world_pos, 1)).xyz * 0.5 + 0.5;
```

Multiplying by the light's view-projection gives the position the light's camera would have rendered this point at
(range -1..1); `* 0.5 + 0.5` turns it into texture coordinates plus a depth (0..1). `shadow_coords.xy` says which
texel to read, `shadow_coords.z` is "my distance", the texel's value is "stored distance".

### Problem: shadow acne, and the two biases

One shadow map texel covers a small *area* of a surface, but stores a single depth. When the surface is tilted
relative to the light, half of that area is slightly further than the stored depth and "shadows itself". The result
is a striped pattern on surfaces which should be fully lit.

```
light rays  | | | |
            v v v v
        ____________      one texel stores the depth of its center (x),
       /   x                the part of the surface on the far side of x
      /                     is "behind" it -> wrongly in shadow
```

Two fixes are combined, both tunable:

* **Normal offset** (`normal_offset`, in texels): the lookup is done for a point moved a bit away from the surface
  along its normal. It's scaled by how steeply the light hits the surface, because that is exactly when the depth
  inside one texel varies the most.
* **Depth bias** (`depth_bias`): a small constant subtracted from "my distance". Handles what is left.

Too little bias gives acne. Too much makes the shadow detach from the object ("peter panning"), so use the smallest
values that look clean.

### Problem: blocky edges, and PCF

Each texel says only "lit" or "shadow", so the shadow edge is a staircase of texels. Blurring the shadow map itself
doesn't help, as an average of depths is not a meaningful depth. Instead the *results of the comparison* are
averaged: **percentage closer filtering**. With `pcf_radius = 1` a 3x3 texel box around the lookup position is tested
and the shadow value is the fraction of tests which passed. Texels on the border of the box are weighted by how much
of them the box covers, so the edge moves smoothly instead of jumping one texel at a time.

`pcf_radius = 0` does a single test and gives hard, blocky edges.

`hard_edges` thresholds the filtered value at one half: the edge is crisp again, but it sits where the filtered shadow
crosses one half, which moves smoothly with the caster instead of jumping one texel at a time. It's meant for the pixel
art look (PIXEL_ART.md), with texels much smaller than a scene pixel.

### Fade out

The shadow map ends at `distance`. To avoid a visible line where shadows suddenly stop, the shadow fades to "lit"
over the last 15% of the distance.

### Settings cheat sheet

| Setting           | Raise it when...                                   | Cost of raising it                    |
|-------------------|----------------------------------------------------|---------------------------------------|
| `map_size`        | shadows are too blocky                             | memory, fill rate                     |
| `distance`        | shadows disappear too close to the camera          | same texels cover more: blurrier      |
| `caster_distance` | shadows of tall/far objects are cut off            | less depth precision                  |
| `normal_offset`, `depth_bias` | lit surfaces show stripes (acne)       | shadow separates from the object      |
| `pcf_radius`      | edges are too hard                                 | (2r+2)^2 texture reads per pixel      |
| `hard_edges`      | edges should be crisp (pixel art), needs `pcf_radius` > 0 | none                           |

---

## SSAO (screen space ambient occlusion)

### The idea

Ambient light is the engine's stand-in for light bouncing around the world and arriving from all directions. A point
in the open receives all of it; a point in a corner or where a foot touches the ground receives less, because part
of the directions is blocked by nearby geometry. Without AO, ambient light is perfectly flat and objects look like
they float.

Checking real geometry around every pixel would be very expensive. The trick of *screen space* AO: the **depth
buffer already is a description of the visible geometry**. So:

1. Render depth of the scene first (**Depth Pre-pass**).
2. For every pixel, take a handful of sample points in a small hemisphere above the surface.
3. For each sample, check the depth buffer: is the sample *behind* what the camera sees there? Then it is inside of
   some geometry, and that direction is blocked.
4. `occlusion = blocked samples / all samples`.

```
          camera
            |
   . . .    |           o = sample inside of the hemisphere, in free space
  . o   o . |           # = sample which ended up inside of the wall -> blocked
 .    o    #|#####
 ____P_____#|# wall     P: 2 of 5 samples blocked -> 40% occluded
```

### Why a depth pre-pass

The Forward pass needs the AO result *while* it shades, but AO needs the depth of the whole scene, which normally is
complete only *after* the Forward pass. So depth is rendered first, on its own, with the cheap `depth_only.fs`.

It has a second benefit. The Forward pass then runs with the depth test "less or equal" against that finished depth
buffer, so the expensive lit shader (it loops over all lights) runs only for pixels which are really visible, exactly
once per pixel.

For that to work, the pre-pass and the Forward pass have to produce *bit-identical* depth for the same triangle. They
use the same vertex shaders and `simple_vertex_definitions.h` declares `invariant gl_Position`, which forbids the
compiler from optimizing the two programs differently.

### Step 1: position and normal from depth (`ssao_common.h`, `ssao.fs`)

There is no G-buffer in this renderer, only depth. That's enough:

* **Position**: pixel coordinates + depth form a point in normalized device coordinates. Multiplying it by the
  inverse of the projection matrix undoes the projection and gives the position in view space (camera at the origin).
  See `view_position_at`.
* **Normal**: positions of the neighbouring pixels lie on the same surface, so two differences (one horizontal, one
  vertical) are two vectors in the surface and their cross product is the normal. At the edge of an object one
  neighbour belongs to something else, so from each pair (left/right, up/down) the one closer in depth is used.
  See `reconstruct_normal`.

### Step 2: the sample kernel (`SsaoPass` constructor)

The kernel is a fixed set of random points in a unit hemisphere, generated once on the CPU. Two details:

* It's a *hemi*sphere pointing along the normal. A full sphere would put half of the samples under the surface and a
  flat wall would come out 50% occluded (the grey look of the first SSAO implementations).
* Samples are pulled towards the center, as geometry near the point matters more than geometry at the edge of the
  radius.

The same kernel for every pixel would produce visible banding with only 16 samples. So each pixel **rotates** the
kernel around its normal by a random vector taken from a tiny 4x4 noise texture repeated over the screen. Neighbours
now use different sample positions: banding turns into fine noise, which step 4 removes.

### Step 3: test the samples (`ssao.fs`)

For each kernel sample: move it to the pixel's hemisphere (`tbn * kernel[i] * radius`), project it back to the screen
to find which pixel it lands on, and compare the sample's depth with the real depth there.

* `bias`: the sample must be clearly behind the visible surface to count. Without it, a flat surface occludes itself
  because of depth precision.
* **Range check**: the depth buffer only knows the *front* of things. A character standing far in front of a wall
  would otherwise darken the wall around its silhouette, since the wall's samples are "behind" the character in the
  depth buffer. So an occluder counts less the further it is from the pixel in depth, relative to `radius`.

Finally `ao = pow(1 - occlusion, power)`; `power` is just a contrast control.

### Step 4: blur (`ssao_blur.fs`)

A 4x4 average, the same size as the noise pattern, so the pattern cancels out exactly. Pixels whose depth differs
too much from the center are left out, otherwise the occlusion of an object would smear onto the background behind
it.

### Step 5: use it (`lit.fs`)

`sample_ambient_occlusion()` reads the AO texture at the pixel being shaded and the value multiplies the **ambient**
term of every light. That is the correct use: direct light is already handled by `diffuse` and by shadows.

`direct_light_occlusion` (0 by default) additionally applies AO to diffuse and specular light. It's physically wrong,
but gives a stronger, more stylized contact darkening, so try it for the art style.

### Limits of the technique

All of them come from "the depth buffer is the only geometry":

* Nothing off screen or hidden behind another object can occlude. Occlusion can pop in at the screen borders.
* Sprites and debug lines are not in the pre-pass: they neither occlude nor receive AO.
* The reconstructed normal is the flat normal of the triangle, not the smooth shading normal. For AO that's fine.

### Settings cheat sheet

| Setting         | Effect                                                                        |
|-----------------|-------------------------------------------------------------------------------|
| `radius`        | size of the effect in world units: small = tight contact shadows, large = soft room corners |
| `bias`          | raise if flat surfaces get dirty, lower if thin creases lose their occlusion  |
| `power`         | contrast / strength                                                           |
| `sample_count`  | quality vs. cost (texture reads per pixel), max 32                            |
| `blur`          | switch off to see the raw noise pattern                                       |
| `direct_light_occlusion` | how much AO also darkens direct light                                |

---

## Notes for the pixelated look

Nothing here is tied to the screen resolution. Scene targets and the AO texture take their size from the render graph
descriptions in `Renderer::add_scene_passes`, so rendering the scene to a small target and upscaling it with nearest
filtering in the Present pass pixelates shadows and AO together with everything else (PIXEL_ART.md). Don't make shadow
map texels as big as scene pixels (small `map_size`, `pcf_radius = 0`): the two grids alias into ragged, jittering edges.
Use small texels with `hard_edges` instead, as the pixel art preset does. SSAO `blur` off gives a dithered occlusion pattern.

## Possible next steps

* A "casts shadows" flag per light and per mesh (today: first directional light, all meshes).
* Cascaded shadow maps: several shadow maps for near/far ranges, for sharp shadows at long view distances.
* Shadows of point and spot lights (cube map / perspective shadow map).
* SSAO at half resolution with a depth aware upsample, if it shows up in the profiler.
* Exposing `RenderSettings` in the C API, so the game can set quality presets.
