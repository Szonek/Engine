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
| Forward / Sprites                           | built-in, per scene camera | SceneColor (+EntityId, SceneDepth) |
| Transparent Depth Copy                      | built-in, per scene camera, only with soft particles | SceneDepthCopy (R32F) |
| Transparent                                 | built-in, per scene camera, only with particles | SceneColor (depth tested, not written) |
| Debug Lines                                 | built-in, per scene camera | SceneColor, SceneDepth            |
| Debug Lines Pick                            | built-in, per scene camera, only with owned lines (gizmos) | EntityId |
| *features of stage* `ePostProcess`          | `RenderFeature` | usually SceneColor                |
| Present                                     | built-in      | SceneOutput                         |
| *features of stage* `eOverlay`              | `RenderFeature` | SceneOutput (game UI)             |
| *features of stage* `eScreen`               | `RenderFeature` | Backbuffer (editor)               |

The scene is rendered at the size of the image it is shown in: the window, or with the editor the scene panel.
`SceneOutput` is then the backbuffer, or with the editor a texture it shows in that panel (`Renderer::set_scene_output`);
`Application::get_scene_viewport()` is that part of the window, which cameras, the game UI and mouse coordinates use.

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
| `transparent_stage.h`             | Declares the Transparent pass of a view (`add_transparent_pass`). **No GL.**      |
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

## The Transparent stage

Blended geometry is drawn after the opaque passes (Forward, Sprites) and before the debug lines and the post-process
features: `add_transparent_pass` (GL-free, `transparent_stage.h`, unit tested) declares a pass which loads SceneColor
and SceneDepth (so it never makes the opaque passes unnecessary) and writes no entity ids (transparent things can't be
picked); its draw code sets the blending and turns depth writes off, so it is depth tested against the opaque scene.
Today it draws particles (`particles.vs/.fs`): the scene submits `SceneRenderData::particles` with a `ParticleBatch`
per emitter (`Scene::particles()`, see `impl/particles.h`); `Renderer::sort_particles` sorts them back to front for each
camera (the batches by their emitter's distance and sort order, then each batch's particles) into the view's part of
the frame's particle SSBO (one per frame in flight, like the lights and bone matrices), and each batch is one instanced
draw of camera-facing quads (colour times texture, or a soft circle), with alpha, premultiplied or additive blending.
It is declared only for a view whose scene has particles. Later translucent things (water, glass) draw in the same stage.

What a particle batch can ask for (the effect's emitter, `ParticleEmitterDesc`):

* **Flipbooks**: the texture is columns x rows frames, left to right and top to bottom; each particle carries the frame
  it shows and the next one with how far it is blended into it (`particle_flipbook_frame`, GL-free and unit tested), so
  the frames cross-fade instead of popping.
* **Erosion**: the texture's alpha is its density, thresholded by the particle's erosion (`particle_erosion`, 0..1 over
  its life) and stretched back to full strength above it, so a puff dissolves from its thin edges inwards.
* **Lit** particles are shaded as the ball of smoke they show: the normal of a sphere in front of the quad (its centre
  facing the camera, its rim along the quad's edge), wrapped diffuse light of the directional lights (smoke lets light
  through, so its shadow side isn't black) plus their ambient, and the first light's shadow map at the fragment, so
  smoke drifting into a building's shadow darkens. The pass then reads the view's shadow map (`TransparentInputs::shadow_map`);
  point and spot lights don't light particles yet.
* **Soft** particles fade out over `soft_distance` in front of the opaque geometry behind them instead of cutting a hard
  line where a quad goes through the floor. A texture can't be sampled while it is the pass's depth attachment, so when
  a batch of the view is soft (`TransparentInputs::needs_scene_depth`) a Transparent Depth Copy pass first copies
  SceneDepth into an R32F texture (`depth_copy.fs`), which the particle shader samples and turns into view depth with
  the inverse projection (perspective and orthographic alike).

The particle texture and the depth copy have their own binding slots (`TEXTURE_PARTICLE`, `TEXTURE_SCENE_DEPTH`). The
smoke puff flipbook is generated (`assets/models/particles/generate_particle_textures.py`, standard library only, the
same bytes everywhere).

## Adding rendering without touching the renderer

1. Derive from `RenderFeature` (or use `CallbackRenderFeature` for something small) and declare passes in `add_passes(FrameContext&)`.
   `FrameContext` gives the graph, the size of the scene image, scene color/depth/entity id handles, the scene output, the backbuffer and the cameras of all submitted scenes.
2. Register it: `renderer.add_feature(feature, RenderStage::ePostProcess, order)`.
3. Switch it on/off at runtime with `feature->set_enabled(bool)` - disabled features declare nothing.

Use `RenderStage::eOverlay` + `add_overlay_pass(frame, name, frame.scene_output, ...)` for anything drawn on top of the presented scene (the game UI is done this way),
and `RenderStage::eScreen` with `frame.backbuffer` for what is drawn around it (the editor).

New data from the game world (e.g. decals) is added to `SceneRenderData`, filled in `Scene::submit_render_data()` and consumed by a pass.
A new built-in lit/unlit shader is a `Material*` class in `material.h/.cpp` plus a draw call in a pass.

## Notes

* Entity ids for picking come from the **previous** frame: `Renderer::read_entity_ids()` reads a few pixels around the click from the
  extracted `EntityId` texture, with the owners of that frame's debug lines. Forward writes the meshes' ids, Debug Lines Pick the
  owners of debug lines (gizmos, `engineDebugDrawSetOwner`), which the editor prefers within a few pixels (`pick_game_object`).
* Geometry/Texture pointers in draw items are only valid until the end of the frame.
* Meshes are drawn by three passes (Shadow Map, Depth Pre-pass, Forward). The Forward pass uses depth test "less or equal" against the
  depth of the pre-pass, so a new mesh type has to be drawn in `draw_meshes_depth_only` too, with the same vertex shader as its lit draw.
* Bone matrices of all skinned meshes of a scene are uploaded once per frame (`SceneGpu::frames[...].skinning`, one copy per frame in flight guarded by a fence, so the CPU never overwrites matrices the GPU is still reading), draws only pass an offset into it.
* `RenderGraph` is unit tested without a GL context: see `src/tests/unit_tests/render_graph_tests.cpp` (target `engine_logic_tests`).
  The same target contains tests of the shadow map math (`shadows_tests.cpp`).
