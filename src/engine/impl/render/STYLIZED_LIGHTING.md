# Stylized lighting

An optional "painted" look of the lit shaders, meant for low-poly assets textured with a flat color palette: the
textures carry no detail, so the lighting has to make the shapes read. It changes only how the existing lights are
shaded, there are no new passes, lights or textures.

Off by default. **Editor: debug panel → Stylized lighting** (next to Shadows / SSAO) switches it on and tunes it live.
The settings are global (`RenderSettings::stylized`, like shadows and SSAO), not saved anywhere and not in the C API:
once a look is chosen, put the values into the defaults of `StylizedLightingSettings`.

| File                                    | What lives there                                                               |
|-----------------------------------------|--------------------------------------------------------------------------------|
| `render_settings.h`                     | `StylizedLightingSettings`: the parameters, documented                         |
| `render_scene_data.h` / `renderer.cpp`  | parameters packed into `ViewLightingGpuData` (the per camera lighting UBO)     |
| `assets/shaders/shadow_helpers.h`       | the matching `ViewLightingData` uniform block                                  |
| `assets/shaders/lit_helpers.h`          | `stylized_*` functions, used by `calc_dir_light` / `calc_point_light` / `calc_spot_light` |
| `assets/shaders/lit.fs`                 | picks the shading normal (flat shading), adds the rim light; `rim_mask` is set by the material |
| `material.cpp`                          | `rim_mask`: 1 for skinned meshes, 0 for static meshes                          |

`lit_helpers.h` reads the uniform block, so the lit shaders are built from `shadow_helpers.h`, `lit_helpers.h`,
`lit.fs` in this order.

## The parts

Each part has a neutral value. With all of them neutral (or stylized lighting disabled) the result is exactly the
standard lighting.

| Part                 | What it does                                                                                     | Neutral |
|----------------------|--------------------------------------------------------------------------------------------------|---------|
| Flat shading         | Faceted low-poly look: every triangle is shaded with its own normal (`cross(dFdx(pos), dFdy(pos))`, flipped to the side of the mesh normal) instead of the smooth normal of the mesh, so assets with smooth normals (KayKit) look faceted without re-exporting them. `flat_shading` blends between the two. The shadow lookup uses the same normal. | 0 |
| Hemisphere ambient   | Ambient light is multiplied by the sky color on surfaces facing up and by the ground color on surfaces facing down, blended by the normal's Y. Tops and sides read differently even in full shadow. | sky and ground white |
| Light ramp           | The diffuse light falls into `bands` steps of equal height instead of a smooth falloff: lit / half lit / unlit regions with soft edges (`band_softness`, 0 is hard cel shading). The first step starts at the terminator. | `bands` 0 |
| Tinted shadows       | Surfaces the directional light doesn't reach (facing away or in a shadow) still get `shadow_color` × its light, scaled by how unlit they are, so shadows are colored instead of only darker. Dimmed by ambient occlusion. Directional light only. | black |
| Rim light            | `rim_color × rim_strength × (1 - N·V)^rim_power` is added where the surface turns away from the camera. Only on skinned meshes (characters) unless `rim_on_static_meshes`: with a top-down camera every wall is seen at a grazing angle and would light up. Independent of the lights. | strength 0 |

Flat shading, hemisphere ambient and the light ramp apply to directional, point and spot lights.

Diffuse with `n` bands (`t = N·L × n`, `w = band_softness / 2`):

```
lit = (smoothstep(-w, w, t) + smoothstep(1 - w, 1 + w, t) + ... + smoothstep(n-1 - w, n-1 + w, t)) / n
```

## Tuning notes

- The ramp brightens lit surfaces (everything with N·L above the last step gets the full light), so the defaults
  compensate with an ambient below 1. They keep the overall brightness of the standard lighting with the light of the
  test level (ambient 0.45, diffuse 0.75). A level with different light intensities needs other values.
- Hard edges (`band_softness` near 0) look noisy on faceted low-poly meshes and flicker on animated characters: neighbouring
  faces flip between steps. Keep some softness, or fewer bands.
- Flat shading plus a light ramp with hard edges quantizes every face to one of the steps, which gives the strongest
  "faceted" look; softer edges keep some gradient across neighbouring faces.
- Debug views (Ambient occlusion / Shadow) still show only that term, stylized or not.
