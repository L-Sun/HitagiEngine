# Toon renderer

`render::toon::ToonRenderer` is a concrete pass builder alongside PBR and
deferred. It owns shading and graph construction; it does not import USD,
evaluate animation, or own skeletons. The USD Viewer selects it with
`--renderer=toon` or its **Toon** button.

## Assets, instances and frame submission

Mesh, Skin, AnimationClip and Material remain shareable assets. ECS animation
and skinning systems produce a separate deformed vertex array for each
`SkinnedMesh` instance. After `scene.GetWorld().Update(...)`, the host gathers
`render::RenderDrawItem::FromEntity(entity)` and submits the resulting frame.
PBR and Toon use the same neutral `RenderView` and `RenderDrawItem` definitions
in `render:scene`; the existing PBR names remain aliases.

The draw item retains its instance's deformed stream. Attributes absent from
that stream, such as UVs, come from the original Mesh. Two entities sharing a
Mesh can therefore display different poses without duplicating that asset or
giving the renderer any knowledge of animation clips.

```cpp
import render;

hitagi::render::toon::ToonRenderer renderer(load_context);
std::vector<hitagi::render::RenderDrawItem> items;
items.push_back(hitagi::render::RenderDrawItem::FromEntity(entity));
hitagi::render::RenderContext rendering(load_context.device, load_context.queues, load_context.bindings);
rendering.Submit(renderer.Capture({.view = view, .draw_items = items}), output_texture);
rendering.RenderPending(); // RenderPlugin schedules this when using App
```

`Capture` copies frame spans and retains shared resources; it borrows the renderer.
It does not deep-copy shared material or mesh contents. The host serializes
resource mutation and submission. RenderContext owns offscreen execution and,
when constructed with a PlatformHost pointer, window resize and presentation.
Build(graph, request) adds passes and returns color/depth handles; execute the
graph before another Build on the same renderer. With App, install RenderPlugin
followed by ToonPlugin to create these resources in the render World.

## Rendering

1. **ToonForward** draws opaque/cutout surfaces with a directional light.
   Normal/light angle selects shadow, midtone and lit bands. Colored shadows,
   thresholded highlights and a restrained rim term preserve the authored
   texture palette. The normal transform uses the inverse transpose of the
   instance transform. Back faces receive flipped normals.
2. **ToonOutline** expands back-facing geometry in projected pixel space.
   Width is specified in output pixels, independent of distance and aspect
   ratio. It tests against the completed surface depth, without writing depth.
   Both passes apply the same base-texture alpha and opacity cutout.
3. By default, both passes render at twice the output width and height.
   **ToonSupersamplingResolve** averages four color samples and selects the
   nearest of their depths. The returned depth has the output dimensions and
   can occlude subsequent passes. A later pass must use `MoveFrom` handles to
   preserve earlier color/depth contents.

This spatial supersampling costs four raster samples per output pixel, plus
the outline draw and resolve. Construct with `{.supersampling = false}` to
disable it. Threshold transitions also use screen derivatives to limit aliasing.
There is no temporal history or motion-vector requirement.

The output contract is `R8G8B8A8_UNORM` color and `D32_FLOAT` depth. Colors are
treated as display/authored sRGB values for this artistic ramp; this is not an
HDR linear-light pipeline. DX12 depth textures requested with `DepthStencil |
SRV` now use a typeless resource with compatible typed depth and shader views,
allowing the resolve to sample depth. Material texture views are explicitly
declared as graph reads and retained through execution.

## Materials

`CreateToonMaterial(desc)` creates a runtime material with both passes, without
an editor dependency. `CreateToonMaterialPasses()` lets a host share pipelines
across many materials while providing independent parameter values.
`ToonForwardPassTemplate()` and `ToonOutlinePassTemplate()` expose the renderer's
Slang shell for material specialization. The runtime factory generates its
packed parameter ABI with the existing material layout utilities.

| Parameter | Default | Meaning |
| --- | --- | --- |
| `base_color`, `base_color_texture` | white, absent | Multiplied together, including alpha |
| `shadow_color` | `(0.63, 0.65, 0.78)` | Shadow-band multiplier |
| `shadow_threshold`, `light_threshold` | `0.1`, `0.65` | Normal/light dot-product boundaries |
| `band_softness` | `0.035` | Minimum transition half-width |
| `specular_threshold`, `specular_strength` | `0.96`, `0.08` | Highlight threshold and amount |
| `rim_strength` | `0.06` | Lit silhouette accent |
| `outline_color`, `outline_width` | dark violet, `1.0` | Outline color and output-pixel width; zero disables |
| `opacity`, `alpha_cutout`, `alpha_cutoff` | `1`, false, `0.5` | Coverage cutout, shared by surface and outline |

The Viewer adds Toon defaults and passes during its existing material
conversion, preserving imported base color, base texture and cutout values.
Changing renderers preserves the scene, camera, and animation play/pause state.
The inspection light is directional for Toon; its intensity is an artistic
multiplier, so PBR and Toon light-slider values are not photometric equivalents.

## Scope and verification

This is a general Toon foundation. It does not reproduce MMD toon/sphere maps,
face SDF shadows, anisotropic hair highlights, cast shadows, facial morphs or
order-independent transparency. Fractional opacity is not alpha blended;
use cutout materials. Inverted-hull outlines depend on mesh winding and normals,
can expose hard normal seams, and do not find interior alpha-mask contours.
Character-specific face/hair materials remain future shading work.

GPU tests on Windows DX12 and Vulkan cover the three lighting bands, optional
supersampling, depth reuse, silhouette expansion, scalar/texture cutouts,
submission retention, and independent poses from a shared Mesh. Viewer E2E
tests exercise switching into Toon, paused frames and resumed skeletal
playback, then replay the complete journey with identical state and PNG bytes.

```powershell
xmake build unit_tests
xmake r unit_tests --gtest_filter="*ToonRendererTest*:*PbrRendererTest*:*SkinningRendererTest*"
xmake build usd-viewer-e2e
xmake r usd-viewer-e2e DX12
xmake r usd-viewer-e2e Vulkan
# Record 1200 animation frames after the fixed 20 setup ticks, at light intensity 1.
xmake r usd-viewer-e2e DX12 --record-animation path/to/scene.usda 1200 0 toon
```
