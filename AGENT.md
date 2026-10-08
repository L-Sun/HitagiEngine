# AGENT.md

This file provides guidance to AI coding agents when working with code in this repository.

## Build System

This project uses **XMake** as its build system. All commands below assume you are in the project root.

### Configure & Build

```bash
# Configure (debug mode, Clang-cl)
xmake f -m debug --toolchain=clang-cl

# Configure (release mode)
xmake f -m release --toolchain=clang-cl

# Build all targets
xmake

# Run a target
xmake r playground
xmake r editor
```

### Compiler Options

```bash
# Enable Tracy profiling
xmake f -m debug --toolchain=clang-cl --profile=y

# Disable ISPC acceleration for math
xmake f -m debug --toolchain=clang-cl --ispc=n
```

### Running Tests

Tests are organized into groups. Run individual test targets with:

```bash
xmake r math_test
xmake r memory_test
xmake r memory_benchmark
xmake r file_io_manager_test
xmake r timer_test
xmake r gfx_test
xmake r shader_compiler_test
xmake r device_test
xmake r render_graph_test
xmake r gui_test
xmake r renderer_test
```

Also, you can run test with group flag like this:
```bash
xmake r -g test/math
xmake r -g test/renderer
xmake r -g test/gui
xmake r -g test/ecs
xmake r -g test/asset
xmake r -g test/* # for all groups
```

To build only a specific target group: `xmake build -g test/math`, `xmake build -g test/gfx`, `xmake build -g test/gui`, etc.

### Test Artifacts

Save any test artifacts generated during agent runs under ./temp.

### Debugging With LLDB

On Windows, the bundled `lldb.exe` may fail with `unable to find 'python311.dll'`. Before using `lldb`, provide a Python 3.11 runtime through `uv` and add it to the current PowerShell session's `PATH`:

```powershell
uv python install 3.11
$pythonDir = Split-Path (uv python find 3.11)
$env:PATH = "$pythonDir;$env:PATH"
lldb --batch -o "run --frames 3 --exit-after-load" -o "bt" -- build\windows\x64\release\editor.exe
```

## Architecture Overview

### Product Scopes

Use these four scopes when deciding where code belongs:

| Scope | Namespace / owner | Purpose | May depend on |
| --- | --- | --- | --- |
| `engine` | `hitagi::` | Runtime engine foundation: core, platform, gfx, render graph, render, ecs, physics, gui, and runtime assets. | Third-party runtime libraries only |
| `editor` | `editor::` | Generic editor and authoring infrastructure: USD/document-style asset editing, inspectors, viewports, cook/import/export tools. | `engine` |
| `game` | `game::` | A specific game's runtime code: gameplay, game assets, renderer policy, render graph/pass-contract choices. | `engine` |
| `game-editor` | `game_editor::` | A specific game's editor application. Combines generic editor tooling with the game's runtime/content rules. | `engine`, `editor`, `game` |

Runtime packaging should only require:

```text
engine + game + cooked assets
```

It should not require:

```text
editor
game-editor
USD authoring data
editor document/tree state
```

Important ownership boundaries:

- `hitagi::asset` is an engine runtime asset layer. It should contain cooked/runtime-ready assets such as meshes, textures, materials, cameras, lights, and scene data that renderer/game code can consume directly.
- USD import/export, authoring graphs, source document trees, editable material graphs, and save-back workflows belong to `editor` or `game-editor`, not to `engine`.
- Game-specific render pipelines, render graph composition, pass contracts, and material compiler policies belong to `game`; the engine should provide gfx/render primitives and runtime asset containers.
- `game-editor` is the integration point that lets editor tooling cook authoring data into `hitagi::asset` objects for preview or packaging.

### Module System

All engine code uses **C++23 modules** (`.cppm` files) with the `export module xxx;` pattern. Regular `.cpp` files contain module implementations. Headers (`.h`/`.hpp`) are only used at module boundaries for third-party libraries via `module;` global fragment blocks.

### Namespace Style

- **Do not use anonymous namespaces** (`namespace { ... }`) in project C++ code, including tests.
- Place implementation helpers directly in the owning named namespace, or use a descriptive named namespace when grouping is needed. Keep module implementation details unexported.

### Module Layers

Following the layered architecture from *Game Engine Architecture* (Jason Gregory), higher layers depend on lower ones — never the reverse:

```
┌─────────────────────────────────────────────────────┐
│                      engine                         │  ← Engine Shell
├──────────┬─────────────────────────┬────────────────┤
│ debugger │           gui           │     render     │  ← Gameplay Foundations
├──────────┴──────────────┬──────────┴────────────────┤
│          ecs            │           asset           │  ← Scene & Resources
├─────────────────────────┴───────────────────────────┤
│        render_graph     │    gfx (Vulkan · DX12)    │  ← Rendering Engine
├─────────────────────────┴───────────────────────────┤
│         app  (SDL3)     │           hid             │  ← Platform Independence
├─────────────────────────┴───────────────────────────┤
│                        core                         │  ← Core Systems
│           (memory · threading · I/O · timer)        │
├──────────────────────────┬──────────────────────────┤
│           math           │          utils           │  ← Foundation
└──────────────────────────┴──────────────────────────┘
```

### Key Namespaces

| Namespace        | Module                  | Purpose                                        |
| ---------------- | ----------------------- | ---------------------------------------------- |
| `hitagi`         | `engine`, `app`, `core` | Top-level engine, application, runtime modules |
| `hitagi::gfx`    | `gfx`, `gfx.base`       | Graphics device abstraction                    |
| `hitagi::rg`     | `gfx.render_graph`      | Render graph                                   |
| `hitagi::ecs`    | `ecs`                   | Entity Component System                        |
| `hitagi::asset`  | `asset`                 | Asset management                               |
| `hitagi::gui`    | `gui`                   | ImGui integration and GUI draw data generation |
| `hitagi::render` | `render`                | Renderer implementations                       |
| `hitagi::math`   | `math`                  | Math library (row-major matrices)              |

### Core Subsystems

**`hitagi/core`** — `RuntimeModule` is the base class for all engine subsystems. Each module has a `Tick()` method and can contain child sub-modules. There is **no global module registry**: dependencies are declared where they are used — long-lived services through constructors (`AssetManager(FileIOManager&, JobSystem&)`), per-operation executors as call arguments (`World::Update(JobSystem&)`, with a serial `Update()` overload), narrow capabilities as function objects (`ImageLoader`, `JobSubmitter`), and plain data as values (`render::ShaderSource`). `Engine` is the composition root: it owns `MemoryManager`/`FileIOManager`/`JobSystem` as members and exposes accessors (`engine.FileIO()`, `engine.Jobs()`, `engine.Assets()`, ...). Tests own their own instances via gtest fixtures. Includes PMR-based memory allocator, file I/O, thread pool, and timer. Full migration notes and diagrams: [docs/architecture/dependency_injection.md](docs/architecture/dependency_injection.md).

**`hitagi/gfx`** — Graphics abstraction with DX12 and Vulkan backends. Create a device with `gfx::create_device(Device::Type::Vulkan)`. The backend is selected from the `AppConfig` supplied by the host application. Matrices are **row-major** on both CPU and GPU.

**`hitagi/gfx/render_graph`** — Frame-scoped render graph (`rg::RenderGraph`). Resources are created/imported each frame via typed handles (`TextureHandle`, `GPUBufferHandle`, etc.), passes are recorded via `RenderPassBuilder`/`ComputePassBuilder`, then `Compile()` + `Execute()` runs the graph.

**`hitagi/ecs`** — Archetype-based ECS. `ecs::World` owns `EntityManager` and `SystemManager`; parallel task scheduling via Taskflow.

**`hitagi/platform`** — `Application` base class with SDL3 backend. Created via `Application::CreateApp(config)`. Host applications own config persistence.

**`hitagi/asset`** — Engine runtime asset module for meshes, textures, materials, cameras, lights, transforms, and asset management. Runtime assets expose `Load(gfx::Device&)` / `Unload()` and are intended to be directly consumable by renderer/game code. USD authoring, editable asset documents, source graph preservation, and cook/export workflows should live in `editor` or `game-editor`, not in this engine module.

**`hitagi/gui`** — Runtime ImGui integration layer. `GuiManager` owns the ImGui context, input mapping, font loading, and queued GUI draw tasks. After `ImGui::Render()`, it converts ImGui output into `gui::GuiDrawData`, including copied vertices, indices, draw commands, the CPU font atlas view, and render graph texture references encoded via `GuiManager::ReadTexture()`. Generic editor UI composition belongs to `editor`; game-specific editor panels belong to `game-editor`.

**`hitagi/render`** — Engine renderer primitives and default renderer implementations. `IRenderer` has concrete implementations such as `ForwardRenderer` and `DeferredRenderer` (G-Buffer MRT pass + fullscreen lighting pass). Game-specific render graph composition, pass-contract policy, and material compiler policy belong to `game`; engine render code should stay generic unless a default renderer explicitly owns the behavior. GUI rendering is explicit: renderer code consumes `const gui::GuiDrawData&` through `IRenderer::RenderGui(...)` and must not query ImGui state or own a `GuiManager`.

**`hitagi/engine`** — Top-level `Engine` class that composes everything. Initialized from a caller-supplied `AppConfig`. Usage pattern: construct `Engine`, call `engine.Tick()` in the game loop.

### Application Entry Pattern

```cpp
import engine;
import asset;

int main() {
    hitagi::Engine engine;
    // ... set up scene, camera, etc. ...
    while (!engine.App().IsQuit()) {
        engine.GuiManager().DrawGui([&]() { /* imgui calls */ });
        // ... render graph setup ...
        engine.Renderer().RenderGui(render_target, engine.GuiManager().GetDrawData(), true);
        engine.Tick();
    }
}
```

### Configuration

The engine consumes `AppConfig` for runtime settings such as `gfx_backend` (`"Vulkan"` or `"DX12"`), window size, asset root path, and log level. Applications decide whether and where to persist that config; the editor uses `editor.json`.

### Adding a New Module

1. Create `hitagi/<name>/` with `.cppm` (interface) and `.cpp` (implementation) files.
2. Add `target("<name>")` in `hitagi/<name>/xmake.lua` with `add_deps(...)` for dependencies.
3. Include the new `xmake.lua` from `hitagi/xmake.lua`.
4. Export the module from `hitagi/engine/engine.cppm` if it should be part of the engine aggregate.

### Platform Notes

- Windows is the primary platform; DX12 backend is Windows-only (`is_plat("windows")`).
- Vulkan backend works on Windows and Linux (Wayland).
- Runtime is set to `MD`/`MDd` (dynamic CRT) on Windows.
- Source files use UTF-8 encoding (`set_encodings("utf-8")`).

