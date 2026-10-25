# USD Viewer

A USD scene and skeletal animation inspector using PocketJS and the built-in PBR, Toon and deferred renderers.

## Run

From the repository root:

```powershell
xmake build usd-viewer
xmake r usd-viewer
xmake r usd-viewer "path/to/my scene.usda" --renderer=pbr --backend=Vulkan
xmake r usd-viewer --backend=DX12 --headless --frames=3
xmake r usd-viewer examples/usd-viewer/assets/skinned.usda
xmake r usd-viewer examples/usd-viewer/assets/skinned.usda --renderer=toon --backend=DX12
```

The default is the included material studio with deferred rendering. **Open USD**
opens a path field and sample shortcuts. Type or paste a path, then press Enter to
commit the field and choose **Open scene**. Relative paths resolve from the working
directory. **Reload** rereads the current file. Invalid files leave the current
scene intact and display an error in the open panel.

## Interaction

| Control | Behavior |
| --- | --- |
| Scene tree | Select objects, expand/collapse hierarchy, scroll long lists |
| Search field | Type a name and press Enter; clear it to restore the hierarchy |
| Inspector | Local position/scale, mesh triangle count, material names, frame/hide/show |
| PBR / Toon / Deferred, or Tab | Select an installed renderer while retaining scene, camera and animation state |
| Lit / Color / Normals / Roughness | Deferred output selection; unavailable modes are disabled |
| Left mouse drag in viewport | Orbit |
| Middle mouse drag in viewport | Pan |
| Wheel in viewport | Zoom; scrolling the tree does not affect the camera |
| Frame / F | Frame the selection, or the whole scene when nothing is selected |
| Reset | Restore the whole-scene view |
| Front / Right / Top | Change viewing direction |
| Light − / + | Adjust the inspection light |
| Play / Pause, Restart | Control all imported skeleton players; restart preserves paused/playing state |
| Physics: On/Off | Enable imported hair/garment bone rigs; restart clears simulation history |
| Escape | Clear selection; in a text field, cancel editing |

The inspector hides on narrow windows to preserve viewport space. Controls use
logical coordinates with uniform OS DPI scaling. Text fields support the engine's
native text/IME session path. Keyboard shortcuts are suppressed while editing.

## Responsibilities

| File | Responsibility |
| --- | --- |
| `main.cpp`, `options.cpp` | Runtime / App / DefaultPlugins assembly, command-line options, run loop |
| `viewer.cpp` | Viewer plugin, injected services, concrete renderer selection, frame coordination |
| `document.cpp` | Scene replacement, hierarchy, selection, visibility, commands |
| `scene.cpp`, `scene_loader.cpp` | Scene loading interface, USD import and material conversion |
| `camera.cpp` | Orbit/pan/zoom controller using `asset::CameraComponent` projection settings |
| `presentation.cpp` | Scene and GUI composition in RenderContext's shared graph; capture |
| `ui_bridge.cpp` | Host state and named layout/control effects |
| `ui/` | PocketJS layout, inspector, reusable controls and text editing |
| `test/e2e.cpp` | Tick driver, interaction journey, pixel/state replay checks |

USDViewerPlugin installs RenderPlugin and the PBR, Toon and deferred plugins.
The main World handles input, scene updates and selecting the frame builder.
RenderApp owns the concrete renderer resources and RenderContext, which builds
scene and GUI passes into one graph and owns the swapchain and presentation.
The viewer publishes after rendering in the child World's Last group. Switching
renderers selects an installed resource. App itself has no renderer accessor or
special renderer slot. The viewer owns its inspection camera projection and pose;
scene cameras are ordinary CameraComponent / Transform components.

The importer supports static/skinned meshes and basic `UsdPreviewSurface` color,
base-color texture, metallic, roughness, opacity and emissive values. This is a
read-only inspector: visibility is a viewing override, not a saved USD edit.
Authored lights/cameras, arbitrary material networks and
viewport picking are not implemented. Selection uses the scene tree. PBR and
deferred have different lighting/display pipelines, so their final colors are
not expected to match.

**Toon** uses three-band lighting, colored shadows, restrained highlights and
pixel-width inverted-hull outlines. Four-sample spatial supersampling smooths
silhouettes and thin details. Imported base-color textures and opacity cutouts
are shared with the other renderers; Toon-specific material defaults are added
by the Viewer material converter. Animation and skinning stay in ECS, and all
renderers consume the same per-instance deformed vertices. See the
[Toon renderer contract](../../docs/architecture/toon_renderer.md) for API,
material controls, costs and limitations.

**Open USD → Skeletal animation** loads two native USD instances sharing mesh
data and playing different clips. UsdSkel bindings become Hitagi joint entities,
shared Clip/Skin assets and per-instance playback/skinning components. Playback
loops over the stage range using `timeCodesPerSecond`; the Viewer updates deformed
bounds for framing. See the [USD import contract](../../docs/assets/animation.md#usd-import-and-preview)
for supported bindings and explicit limitations, including four nonzero
influences per point, classic linear skinning, and no blend shapes. The editor
Cook API supports HTGC v8 animation export; this Viewer has no export UI.

## Deterministic E2E

See [runtime character physics](../../docs/assets/runtime_character_physics.md) for
the Jolt/Bullet hair and garment rig, backend comparison and conversion without baked physics.

```powershell
xmake build usd-viewer-e2e
xmake r usd-viewer-e2e Vulkan
xmake r usd-viewer-e2e DX12
```

The approach follows PocketJS's [deterministic UI runtime](https://pocketjs.dev/blog/ui-runtime-that-cant-flake/):
explicit inputs and virtual frames replace wall-clock waiting.

Each `Step` applies one named action, advances input once, advances the viewer
once, waits for its GPU execution/readback, and records state plus a PNG. A click
has separate press and release ticks. Text replacement is one ordered text-input
transaction. Planned idle ticks expose the following UI snapshot; there are no
poll-until-success loops, sleeps, or retries.

`GuiAppOptions::single_step` makes each `PrepareFrame()` / `Compose()` pair advance
exactly one PocketJS frame. Ordinary application execution retains the existing
clock accumulator. The test owns a hidden native window, injects input through
`InputState`, and does not pump the uncontrolled OS input queue. Resize and
Windows close messages are explicit synchronous actions. Asset loading is
synchronous, so completion timing is not an unrecorded input.
Skeletal playback advances by exactly 1/60 second per test tick; interactive
playback uses elapsed time from a monotonic clock.

The journey runs twice with fresh GUI, scene, renderer and graphics services.
Every replay tick must match the first run's state **and complete PNG bytes**.
Assertions also check selection, actual viewport changes on hide/show, camera
movement/reset, input isolation, disabled controls, filtering, failed-open
recovery, reloading, renderer switching, compact/wide layouts, and shutdown.
The skeletal sample also verifies visible motion in both renderers, stable
paused pixels and restoration of the initial pose on restart.

Artifacts go to `temp/usd-viewer-e2e/<backend>/`:

- `record.jsonl` and `replay.jsonl`: named actions and complete host snapshots.
- `frame-NNN.png`: each recorded frame, including intermediate input states.
- `diverged-NNN.png`: the first mismatching replay frame, when applicable.

This checks determinism and behavioral assertions; it is not a committed visual
golden baseline. Pixel equality is scoped to the same backend, device, DPI and
build. It does not promise identical pixels across Vulkan/DX12 or GPU drivers.

To record a skeletal playback demonstration from the actual GPU output:

```powershell
xmake r usd-viewer-e2e DX12 --record-animation
ffmpeg -framerate 60 -start_number 1 -i temp/usd-viewer-recording/DX12/frame-%03d.png -c:v libx264 -crf 20 -pix_fmt yuv420p -movflags +faststart temp/usd-viewer-animation.mp4
```

The recording advances the Viewer at 60 fixed ticks per second of video and
captures every frame, including pause, restart and renderer switching. It runs
once without the E2E replay pass; rendering/readback time does not change the
video's playback speed.

To capture a custom USD scene for 1,200 animation ticks instead, pass its path
and the tick count after `--record-animation`. This uses a front view with the
inspection light at 3.0 and writes frames under
`temp/usd-viewer-recording/<scene-stem>/<backend>/`:

```powershell
xmake r usd-viewer-e2e DX12 --record-animation "path/to/scene.usda" 1200
```

An optional final argument sets the number of light increases (0–8, default 8).
Use `1200 0` to retain the default 1.0 light for bright textured models. All
settings keep 20 setup ticks, so frames 21–1220 contain the 20-second clip.
USD `opacityThreshold` enables alpha cutout for textures such as hair details.
