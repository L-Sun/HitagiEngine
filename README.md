<div align="center">
    <h1> Hitagi Engine</h1>
    <div>
        <a href="https://github.com/L-Sun/HitagiEngine/actions?query=workflow%3Awindows-build">
            <img alt="GitHub Workflow Status" src="https://img.shields.io/github/actions/workflow/status/L-Sun/HitagiEngine/windows.yaml?logo=windows&style=flat-square">
        </a>
    </div>
    <hr>
</div>

Hitagi Engine is an experimental game engine written with C++23 modules and built with [XMake](https://xmake.io).

## Features

- **Core**: pmr-based memory pools, file I/O, a [Taskflow](https://github.com/taskflow/taskflow)-based job system, timers, and a runtime module tree composed by `Engine` with explicit dependency injection
- **Math**: a 3D math library with swizzle support and optional [ISPC](https://github.com/ispc/ispc) acceleration; matrices are row-major on both CPU and GPU
- **Graphics (gfx)**: a graphics API abstraction with DX12, Vulkan, and mock backends, bindless resources, and a render graph
- **Render**: reusable rendering interfaces, render-graph integration, GUI and text rendering; optional PBR forward and deferred rendering presets live outside Engine
- **Asset**: runtime resources (textures, materials, meshes, cameras, lights, scenes) loaded from a cooked binary format, with GPU residency management in `AssetManager`
- **ECS**: archetype-based entities, filters, and system scheduling
- **Physics**: integration of [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
- **GUI and input**: [PocketJS](https://pocketjs.dev/) with Solid/TSX components and engine HID input
- **Platform**: Win32 on Windows, SDL3 elsewhere
- **Editor Framework**: reusable editing workflows and extension points, including an asset browser, scene viewport, inspectors, OpenUSD import, material graphs and validation, and asset cooking; concrete shader generation is supplied by games or optional presets

Platform status: Windows is the primary target and the only one covered by CI. Linux/Wayland code paths exist for Vulkan and SDL3 but are not regularly validated.

## Repository Layout

| Path | Contents |
| --- | --- |
| [hitagi/](hitagi) | Engine modules (`core`, `math`, `gfx`, `render`, `asset`, `ecs`, `physics`, `gui`, `hid`, `platform`, ...) and the editor library |
| [hitagi/interop/](hitagi/interop) | Module bridges for third-party dependencies (`interop.<library>`) |
| [presets/](presets) | Opt-in rendering models and authoring companions; Engine and Editor Framework do not depend on them |
| [examples/](examples) | Editor entry point and PBR demo |
| [assets/](assets) | Shaders, fonts, materials, scenes, and cooked assets |
| [docs/](docs) | Architecture, asset, and development documentation |
| [tools/](tools) | Python helpers for Blender/USD authoring and asset validation |
| [xmake/](xmake) | Local package recipes, rules, and build scripts |

## Building

### Requirements (Windows)

1. [Visual Studio](https://visualstudio.microsoft.com/downloads/) (or the Build Tools) with:
    - The latest MSVC toolset and the C++ core features
    - C++ Clang tools for Windows, to build with clang-cl
    - The latest Windows SDK
2. [XMake](https://xmake.io/#/guide/installation)
3. [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
4. [ISPC](https://github.com/ispc/ispc/releases), available on `PATH` (optional; disable with `--ispc=n`)
5. [Rust](https://rustup.rs/) (MSVC target) and [Bun](https://bun.sh/) for PocketJS

Other third-party dependencies are downloaded and built during configuration/build, so keep a network connection available. OpenUSD belongs to the editor authoring target. PocketJS's runtime and host-side UI compiler are pinned, checksum-verified local recipes under `xmake/packages/p/`, installed and cached by XMake. Cargo builds the runtime with `-j 1`; Bun and Cargo must be available on `PATH`.

### Configure and Build

```powershell
git clone https://github.com/L-Sun/HitagiEngine
cd HitagiEngine

# Use -m release for an optimized build; --toolchain=msvc is also supported.
xmake f -m debug --toolchain=clang-cl
xmake -j 1 editor
```

The default `editor` uses PocketJS for its hierarchy, toolbar, transform inspector and asset browser. C++ owns scene state, commands, undo/redo and isolated Play mode. A native message queue connects UI actions and scene snapshots. The GPU-rendered scene viewport is composited into the UI with matching picking coordinates.

The entry UI lives in `hitagi/editor/ui/hitagi-ui.tsx`. The reusable `hitagi/editor/ui/editor-shell.tsx` accepts game UI extensions; game editors choose their bundle through `EditorLaunchOptions::ui_bundle` / `ui_assets`. The local `pocketjs` package provides the native runtime, TSX tools and exported `@pocketjs/ui` rule. Application targets depend on their own incremental UI resource targets, producing JavaScript and font/style packs under `build/pocketjs/ui/`. Compiler scratch files stay under `build/pocketjs/work/`, not the shared package installation. `GuiManager::LoadApp(js, pak)` explicitly loads a UI.

PocketJS's portable runtime runs QuickJS and the Rust UI core. A package-owned C ABI exports DrawList commands and resources; Hitagi rasterizes the resulting geometry on the GPU. The editor now uses DPI-aware window dimensions, density-2 font resources, resizable panels, a collapsible/scrollable hierarchy and folder/type-filtered assets. Games can retain the default 960×540 letterboxed canvas. Docking, free-form text/IME, drag gizmos and full material/camera/light inspectors are not yet ported; transform fields still use step controls. Scene saving remains unsupported as before.


The default build includes all enabled application, library, UI and test targets. Benchmark targets join the default build when enabled with `--benchmarks=y`.

| Option | Default | Description |
| --- | --- | --- |
| `--profile=y` | `n` | Enable [Tracy](https://github.com/wolfpld/tracy) profiling |
| `--benchmarks=y` | `n` | Enable the `memory_benchmark` and `ecs_benchmark` targets |
| `--ispc=n` | `y` | Disable ISPC acceleration in the math library |

### Running

```powershell
xmake r editor               # PocketJS editor
xmake r pbr-demo-game        # PBR demo; pbr-demo-game-editor opens it in the editor
```

### Testing

```powershell
xmake r editor --headless --gfx-backend Vulkan --frames 3  # short GPU integration check, no visible window
# Add --screenshot temp/pocketjs/gpu.png to inspect the engine's GPU output.

xmake build unit_tests
xmake r unit_tests --gtest_list_tests
xmake r unit_tests --gtest_filter="SuiteName.*"
xmake r unit_tests --jobs=8  # run in parallel shards; add --verbose to enable engine logs

xmake build editor_tests
xmake r editor_tests
```

## Documentation

- [Overall abstraction design](docs/architecture/overview.md): Engine, Editor Framework, Game, and Game Editor responsibilities and integration
- [Dependency injection](docs/architecture/dependency_injection.md)
- [PocketJS integration](docs/architecture/pocketjs.md): illustrated build, input, DrawList adaptation, and GPU rendering flow
- [Asset module](docs/assets/README.md): resource lifecycle, materials, and the cooked binary format
- [Interop boundaries](docs/development/interop.md)
- [Debugging on Windows](docs/development/debugging.md)
- [AGENT.md](AGENT.md): contribution conventions, architecture boundaries, and validation guidance

## License

See [LICENSE](LICENSE).
