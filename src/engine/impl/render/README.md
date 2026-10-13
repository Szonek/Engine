# Rendering architecture

Rendering is built around a **render graph** (frame graph). The game world never calls the graphics API while it
updates; it only *describes* what to draw. The renderer turns that description into passes, and the graph executes them.

```
Scene::update()                           Renderer::render_frame()  (called from Application::end_frame)
  └─ collects SceneRenderData   ───────►    1. upload GPU data (camera/scene UBOs, lights, debug lines)
     (cameras, meshes, lights, ...)         2. declare passes in rg::Graph
     Renderer::submit_scene()               3. Graph::compile()   - cull unused passes, compute texture lifetimes
                                            4. Graph::execute()   - bind FBOs, clear, barriers, run passes
```

Passes declared every frame:

| Pass                                        | Stage         | Writes                              |
|---------------------------------------------|---------------|-------------------------------------|
| Clear Scene Targets                         | built-in      | SceneColor, EntityId, SceneDepth    |
| Shadow Map                                  | built-in, per scene camera | ShadowMap              |
| Depth Pre-pass                              | built-in, per scene camera | SceneDepth             |
| SSAO / SSAO Blur                            | built-in, per scene camera | AmbientOcclusion       |
| Forward / Sprites / Physics Debug Lines     | built-in, per scene camera | SceneColor (+EntityId, SceneDepth) |
| *features of stage* `ePostProcess`          | `RenderFeature` | usually SceneColor                |
| Present                                     | built-in      | Backbuffer                          |
| *features of stage* `eOverlay`              | `RenderFeature` | Backbuffer (UI, editor widgets)   |

Shadows and ambient occlusion (the three passes before Forward) are explained in [SHADOWS_AND_SSAO.md](SHADOWS_AND_SSAO.md).
Shadow Map and SSAO are declared only when enabled in `RenderSettings` (`Renderer::get_settings()`, editable in the editor's debug panel).

The pass list of the last frame (with culled passes greyed out and resource reads/writes in a tooltip) is shown in the
editor's debug panel. Every pass is also a named debug group in RenderDoc/Nsight.

## Files

| File                              | Responsibility                                                                    |
|-----------------------------------|-----------------------------------------------------------------------------------|
| `render_graph.h/.cpp`             | The graph: passes, resources, culling, lifetimes/aliasing, barriers. **No GL.**   |
| `render_graph_gl_backend.h/.cpp`  | OpenGL side: texture pool, FBO binding, viewport, clears, pipeline state reset    |
| `render_scene_data.h`             | Plain data the scene submits (draw items, lights, cameras) + GPU struct layouts   |
| `renderer.h/.cpp`                 | Owns the graph, materials, per-scene GPU buffers; builds the built-in passes      |
| `render_feature.h`                | `RenderFeature` extension point, `FrameContext`                                   |
| `render_settings.h`               | `RenderSettings`: shadow and SSAO parameters                                      |
| `shadows.h/.cpp`                  | Fits the shadow map of the directional light to the camera frustum. **No GL.**    |
| `ssao_pass.h/.cpp`                | Screen space ambient occlusion passes                                             |
| `outline_feature.h/.cpp`          | Example feature: editor selection outline (raster mask pass + compute pass)       |
| `grid_feature.h/.cpp`             | Editor feature: infinite anti-aliased grid on the XZ plane (one full screen pass) |

## Writing a pass

```cpp
struct BlurData { rg::TextureHandle input; rg::TextureHandle output; };

frame.graph.add_pass<BlurData>("Blur",
    // setup: runs immediately, declares what the pass touches
    [&](rg::Builder& builder, BlurData& data)
    {
        data.input  = builder.read(frame.scene_color);
        data.output = builder.write_color(0, builder.create_texture("Blurred", { frame.width, frame.height, DataLayout::eRGBA_U8 }), rg::LoadOp::eDontCare);
    },
    // execute: runs later (only if the pass wasn't culled). Framebuffer, viewport and clears are already done.
    [this](const BlurData& data, rg::PassContext& ctx)
    {
        shader_.bind();
        shader_.set_texture_with_sampler("input_texture", &ctx.texture(data.input));
        quad_.bind();
        quad_.draw(Geometry::Mode::eTriangles);
    });
```

What the builder offers:

* `create_texture` - transient texture. Allocated lazily from a pool just before its first pass and returned after its
  last one, so textures with disjoint lifetimes share memory. Content is undefined until written.
* `read` (sampled), `read_image` / `write_image` (image load/store, e.g. compute shaders).
* `write_color(slot, texture, LoadOp, ClearValue)` / `write_depth(...)` - a pass with attachments is a *raster pass*:
  the graph binds the FBO, sets the viewport to the attachment size, resets pipeline state (depth test/write on, back
  face culling on, blending off) and applies `eClear`. A pass which wants something else (e.g. no culling) sets it itself.
* `set_side_effect()` - never culled.

Culling: walking backwards from outputs (the backbuffer, imported and extracted textures) a pass survives only if it
writes something that a surviving pass needs. A pass that fully overwrites a texture (`eClear`/`eDontCare`)
makes earlier writers of that texture unnecessary. Passes run in declaration order.

Barriers are inserted automatically: before a pass using images, and before a pass that touches a texture written
through an image since the last barrier.

Problems found while building the graph (reading a texture nobody wrote, attachments of different sizes, ...) are logged
once as `[RenderGraph]` errors and the broken pass is skipped.

## Adding rendering without touching the renderer

1. Derive from `RenderFeature` (or use `CallbackRenderFeature` for something small) and declare passes in `add_passes(FrameContext&)`.
   `FrameContext` gives the graph, window size, scene color/depth/entity id handles, the backbuffer and the cameras of all submitted scenes.
2. Register it: `renderer.add_feature(feature, RenderStage::ePostProcess, order)`.
3. Switch it on/off at runtime with `feature->set_enabled(bool)` - disabled features declare nothing.

Use `RenderStage::eOverlay` + `add_backbuffer_overlay_pass(...)` for anything drawn straight onto the screen (UI is done this way).

New data from the game world (e.g. decals) is added to `SceneRenderData`, filled in `Scene::submit_render_data()` and consumed by a pass.
A new built-in lit/unlit shader is a `Material*` class in `material.h/.cpp` plus a draw call in a pass.

## Notes

* Entity ids for picking come from the **previous** frame: `Renderer::read_entity_id()` reads the extracted `EntityId` texture.
* Geometry/Texture pointers in draw items are only valid until the end of the frame.
* Meshes are drawn by three passes (Shadow Map, Depth Pre-pass, Forward). The Forward pass uses depth test "less or equal" against the
  depth of the pre-pass, so a new mesh type has to be drawn in `draw_meshes_depth_only` too, with the same vertex shader as its lit draw.
* Bone matrices of all skinned meshes of a scene are uploaded once per frame (`SceneGpu::skinning_ssbo`), draws only pass an offset into it.
* `RenderGraph` is unit tested without a GL context: see `src/tests/unit_tests/render_graph_tests.cpp` (target `render_graph_tests`).
  The same target contains tests of the shadow map math (`shadows_tests.cpp`).
