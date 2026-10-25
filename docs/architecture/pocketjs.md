# PocketJS integration

PocketJS is the only GUI backend. The integration runs Solid/TSX components in QuickJS. The Rust core computes layout and animation and produces a DrawList; Hitagi converts it into indexed geometry and renders it through its Vulkan/DX12 GUI pass. The production path does not allocate a full-window CPU framebuffer. The upstream software renderer remains available to the native smoke test as a reference.

![PocketJS build, runtime, and presentation flow](../assets/pocketjs_integration.svg)

[PNG preview](../assets/pocketjs_integration.png). The arrows describe artifact and data flow; the runtime boxes are responsibilities, not separate threads.

## 1. Build and explicitly load the UI

[One local XMake package](../../xmake/packages/p/pocketjs/xmake.lua), `pocketjs 0.14.0`, uses the version declared in upstream's `package.json`, pins upstream commit `55e39a06868c9776f0320909ba37b187f0606d0f` and verifies its source archive checksum. Hitagi-specific adaptations live in the package recipe and supporting files; they do not introduce a separate PocketJS version. It provides the native runtime, TSX tools and exported UI rule while keeping package installation separate from application resource compilation:

- [pocketjs](../../xmake/packages/p/pocketjs/xmake.lua) calls Cargo to build the portable `pocket-apple` static library, containing QuickJS, the Rust runtime and Hitagi's DrawList/resource bridge. It installs headers and the library into XMake's package cache and declares the Windows system libraries. The historical name does not imply a UIKit dependency. `hitagi_interop` consumes it through `add_packages`; [interop.pocketjs](../../hitagi/interop/pocketjs.cppm) preserves the C ABI module bridge.
- The same package installs the compiler/framework and locked Bun dependencies under `share/pocketjs`. Installation does not build any Hitagi application. The recipe currently supports native Windows x64 builds; Cargo and Bun must be on `PATH`. Cross-compilation is not implemented.
- The package installs `pocket.h`, containing the C ABI and a handwritten C++ adapter for the pinned SDK's `contracts/spec/spec.ts`. It owns draw enums, command lengths and resource-format helpers; `interop.pocketjs` re-exports them for the draw decoder. Review these definitions against the upstream contract when upgrading PocketJS. Existing package caches need reinstallation when the adapter changes.
- The exported [@pocketjs/ui rule](../../xmake/packages/p/pocketjs/rules/ui.lua) invokes Bun on an application's TSX entry. UI targets use `add_packages("pocketjs")`, `add_rules("@pocketjs/ui")` and the `pocketjs.entry` / `pocketjs.density` values. Output is written under `build/pocketjs/ui/`. Applications and tests explicitly depend on the bundles they use.

The repository's [root TypeScript configuration](../../tsconfig.json) extends `build/pocketjs/tsconfig.json`, which the UI rule generates during XMake configuration with paths to the installed SDK and Solid types. The root config selects project TS/TSX sources and Solid JSX types for all UIs, excluding build outputs, temporary directories and local dependencies. Keep SDK paths in the generated config rather than duplicating them in application configs. Run XMake configuration/build first on a fresh checkout; no separate application `node_modules` installation is needed.

The UI rule records the compiler's input manifest, including imported UI modules, fonts and resource manifests, and tracks its own script as a build input. Unchanged inputs skip bundling; missing outputs force regeneration. UI edits do not rebuild the native dependency. Compiler patches redirect the generated style mirror and transform cache to each target's `build/pocketjs/work/` directory. The installed framework has a placeholder style module for Bun's first-pass resolver; actual styles are injected into each bundle in memory. UI builds do not mutate the shared package installation or depend on `.local/pocketjs`.

A host explicitly calls `GuiManager::LoadApp(js, pak)` to load its bundle. Construction alone does not load a UI. Loading uses a candidate runtime: a failed load preserves the previous working application, while successful loading keeps initial layout effects from the new guest.

## 2. Advance the runtime and collect drawing commands

[GuiManager](../../hitagi/engine/gui/gui_manager.cpp) owns a native runtime created with `pocket_create_gpu`. This constructs the same upstream `Guest` and `UiSurface` as the portable host, without allocating its software framebuffer. Existing load, event and frame APIs remain in use; production code never calls `pocket_apple_render`.

All calls run on the engine thread. The adapter accumulates elapsed time and advances the guest/core at fixed 60 Hz steps, clamping each elapsed-time contribution to 0.1 seconds. Desktop input uses floating-point pointer packets in logical coordinates, wheel messages and keyboard navigation. Fixed-canvas mode retains letterboxing and encoded touch input.

The package-owned [C ABI extension](../../xmake/packages/p/pocketjs/pocket.h) exposes:

- `pocket_draw`: runs `Ui::draw()` and returns the packed DrawList, plus resource slot counts. Drawing can allocate core glyph/disc pages, so resource enumeration follows this call.
- `pocket_texture`: returns a live texture's generation-tagged handle, content revision, format, pixels and sampling mode.
- `pocket_font`: returns an atlas's revision, coverage bytes and logical/coverage cell dimensions.

These are borrowed views, consumed synchronously before subsequent guest mutations. [PocketDrawDecoder](../../hitagi/engine/gui/draw.cpp) copies the command words, decodes them into engine-owned geometry, and caches immutable RGBA8 resource snapshots. Texture identity/revision and font revision determine replacement; unchanged resources share the same snapshot. Each replacement receives a unique engine resource ID, including across UI reloads. Freed core textures leave the current resource set, while older copied frame data retains its shared resources.

The decoder handles rectangles, gradients, glyph runs, textured quads, triangles, textured triangles and nested scissors. It expands 5650, 4444, 8888 and indexed T8 textures once per resource change. Glyph atlases retain bundle density and include transparent gutters for fractional-DPI filtering. Native-provider text and Pocket System compositor surfaces are rejected explicitly: this host installs neither provider. Hosts composite native textures through `DrawTexture`.

`pocket_resize` updates the existing guest and core viewport without reloading components. GPU hosts change layout dimensions without allocating a pixel buffer; the software reference host still resizes its framebuffer. The desktop layout uses window dimensions divided by the OS DPI ratio. Resource density and display scale remain separate: a host can bake density-2 glyphs, and the GPU draws their quads in physical window coordinates.

## 3. Render with Hitagi's GPU pipeline

`GuiManager::Compose()` transforms logical geometry and clip rectangles into display coordinates, then appends frame-local scene/preview texture quads. `GuiDrawData` owns geometry and shared immutable image/font snapshots; it no longer uses a full-screen image in a `font_atlas` field.

[GuiRenderUtils::GuiPass()](../../hitagi/engine/gui/gpu_draw.cpp) caches GPU textures by immutable resource ID. It uploads a resource only when first encountered, drops cache entries absent from the current resource set, and declares all texture accesses through RenderGraph. The graph retains imported resources through execution. After GPU completion, the application renderer clears frame imports so replaced textures can be released; live renderer caches reimport their resources next frame. Fonts use linear filtering; images preserve the core's sampling hint. Solid geometry outputs vertex color directly. The current UNORM target preserves the portable renderer's encoded-byte color blending convention.

Each frame uploads vertex/index and draw-parameter buffers. Consecutive geometry with the same resource and clip rectangle is batched without reordering transparent draws. Hitagi rasterizes those triangles directly into the render target, composites native GPU scene textures and presents the result. There is no UI-wide BGRA-to-RGBA conversion or framebuffer upload. Layout, animation, tree traversal and geometry conversion still consume CPU time; this change does not imply a measured frame-rate improvement.

The application owns both `GuiManager` and `gui::GuiRenderUtils`. After completing the GUI tick, it passes `GuiManager::GetDrawData()` to `GuiPass(graph, target, draw_data, clear_target)` and executes its graph while the draw data remains alive and unchanged. Engine performs no implicit GUI submission. Window resize and native viewport ordering remain as described below.

## Host ordering and validation

Hosts using native texture overlays call `GuiManager::PrepareFrame()` before consuming layout effects and rendering those textures. The later GUI tick composes the prepared frame. Logical coordinates map through uniform DPI scaling.

For deterministic hosts, `GuiAppOptions::single_step` advances exactly one virtual frame per `PrepareFrame()` / `Compose()` pair, without consulting elapsed time. Ordinary application execution still uses the clock accumulator.

The engine retains native modal-resize handling, GUI input, DrawList decoding and GPU rendering. Interactive editor, game-preview and editor resize tests were removed with their host. Current adapter checks run through `unit_tests`:

```powershell
xmake build unit_tests
xmake r unit_tests --gtest_filter="PocketGuiManagerTest.*:PocketGpuRenderTest.*:AppTest.*"
```

## Windows text input and composition

The Windows host consumes `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION` and `WM_IME_ENDCOMPOSITION` while a text field owns input. `GCS_RESULTSTR` produces a commit; `GCS_COMPSTR`, cursor, attributes and clause data produce a preedit update. The host consumes these messages without default result emission, suppresses the default composition window, and leaves the candidate window to Windows. Ordinary `WM_CHAR` input combines UTF-16 surrogate pairs; control characters are handled as editing keys. Composition-owned keys remain consumed until release.

`InputManager::DrainTextInput()` transfers an ordered queue of start/update/commit/end/key/blur events once. Each carries its focus-session id; text and offsets use UTF-16 to match Windows and JavaScript. `GuiManager` sends them through the existing overlay state channel as `kind: "text"`, forces a GUI step when input is pending, and consumes the private `hitagi.text <session> <x> <y> <width> <height> <caret-x> <caret-y>` effect before application effects reach the host. All geometry is logical UI geometry; `DisplayRect()` converts the caret for the native candidate window. Zero session ends input. Session changes cancel native composition. Focus loss, disposal and successful bundle reload release ownership; delayed events cannot modify a different field. Hosts can suppress keyboard navigation and shortcuts during editing, including the frame that ends editing and held confirmation buttons until release.

The former TSX text-field implementation and editor font bundle have been removed. The native text/IME transport remains available to future GUI hosts; SDL text/IME integration remains outstanding.
