# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
```

Also, you can run test with group flag like this:
```bash
xmake r -g test/math
xmake r -g test/renderer
xmake r -g test/ecs
xmake r -g test/asset
xmake r -g test/renderer
xmake r -g test/* # for all groups
```

To build only a specific target group: `xmake build -g test/math`, `xmake build -g test/gfx`, etc.

## Architecture Overview

### Module System

All engine code uses **C++23 modules** (`.cppm` files) with the `export module xxx;` pattern. Regular `.cpp` files contain module implementations. Headers (`.h`/`.hpp`) are only used at module boundaries for third-party libraries via `module;` global fragment blocks.

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
| `hitagi::render` | `render`                | Renderer implementations                       |
| `hitagi::math`   | `math`                  | Math library (row-major matrices)              |

### Core Subsystems

**`hitagi/core`** — `RuntimeModule` is the base class for all engine subsystems. Each module has a `Tick()` method and can contain child sub-modules. All modules are registered in a global map accessible via `RuntimeModule::GetModule(name)`. Includes PMR-based memory allocator, file I/O, thread pool, and timer.

**`hitagi/gfx`** — Graphics abstraction with DX12 and Vulkan backends. Create a device with `gfx::create_device(Device::Type::Vulkan)`. The backend is selected at runtime from `hitagi.json` (`gfx_backend` field). Matrices are **row-major** on both CPU and GPU.

**`hitagi/gfx/render_graph`** — Frame-scoped render graph (`rg::RenderGraph`). Resources are created/imported each frame via typed handles (`TextureHandle`, `GPUBufferHandle`, etc.), passes are recorded via `RenderPassBuilder`/`ComputePassBuilder`, then `Compile()` + `Execute()` runs the graph.

**`hitagi/ecs`** — Archetype-based ECS. `ecs::World` owns `EntityManager` and `SystemManager`; parallel task scheduling via Taskflow.

**`hitagi/platform`** — `Application` base class with SDL3 backend. Created via `Application::CreateApp(config)` or from `hitagi.json`.

**`hitagi/asset`** — `AssetManager` for loading scenes, meshes, materials, textures. Assimp for model import; custom parsers for PNG, JPEG, BMP, TGA. Materials are defined with named instances (e.g., `"Phong"`).

**`hitagi/render`** — `IRenderer` interface; `ForwardRenderer` is the concrete implementation. The renderer owns the swapchain and render graph instance.

**`hitagi/engine`** — Top-level `Engine` class that composes everything. Initialized from `hitagi.json` at the project root. Usage pattern: construct `Engine`, call `engine.Tick()` in the game loop.

### Application Entry Pattern

```cpp
import engine;
import asset;

int main() {
    hitagi::Engine engine;  // reads hitagi.json
    // ... set up scene, camera, etc. ...
    while (!engine.App().IsQuit()) {
        engine.GuiManager().DrawGui([&]() { /* imgui calls */ });
        // ... render graph setup ...
        engine.Tick();
    }
}
```

### Configuration

`hitagi.json` in the project root controls runtime settings: `gfx_backend` (`"Vulkan"` or `"DX12"`), window size, asset root path, log level.

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
