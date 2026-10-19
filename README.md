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
- **Render**: concrete PBR and deferred renderers built on RenderGraph, registered as ordinary runtime modules; GUI and text drawing remain separate composable utilities
- **Asset**: runtime resources (textures, materials, meshes, cameras, lights, scenes) loaded from a cooked binary format, with GPU residency management in `AssetManager`
- **ECS**: archetype-based entities, filters, and system scheduling
- **Physics**: integration of [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
- **GUI and input**: [PocketJS](https://pocketjs.dev/) with Solid/TSX components and engine HID input
- **Platform**: Win32 on Windows, SDL3 elsewhere
- **Asset authoring**: OpenUSD import, material source metadata, renderer-agnostic material graph compilation against renderer-supplied pass templates, and asset cooking

Platform status: Windows is the primary target and the only one covered by CI. Linux/Wayland code paths exist for Vulkan and SDL3 but are not regularly validated.

## Repository Layout

| Path | Contents |
| --- | --- |
| [hitagi/engine/](hitagi/engine) | Runtime modules (`core`, `math`, `gfx`, `render`, `asset`, `ecs`, `physics`, `gui`, `hid`, `platform`, ...), Engine composition, and runtime tests |
| [hitagi/editor/](hitagi/editor) | Editor Framework: USD import, material graphs, and asset cooking |
| [hitagi/interop/](hitagi/interop) | Module bridges for third-party dependencies (`interop.<library>`) |
| [hitagi/engine/render/](hitagi/engine/render) | Concrete PBR and deferred runtime renderers; the PBR material pass template |
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
xmake build -j 1
```

The repository provides engine libraries, built-in renderers, asset authoring tools and tests. `examples/renderer` demonstrates explicit renderer construction and module-tree execution. The interactive editor has been removed; the `editor` C++ module / `hitagi_editor` library retains USD import, cook, and material graphs with a shading-model-agnostic compiler. Engine GUI support remains available.

The default build includes enabled libraries and tests. Benchmark targets join it with `--benchmarks=y`.

| Option | Default | Description |
| --- | --- | --- |
| `--profile=y` | `n` | Enable [Tracy](https://github.com/wolfpld/tracy) profiling |
| `--benchmarks=y` | `n` | Enable the `memory_benchmark` and `ecs_benchmark` targets |
| `--ispc=n` | `y` | Disable ISPC acceleration in the math library |

### USD Viewer

```powershell
xmake build usd-viewer
xmake r usd-viewer path/to/scene.usda --renderer=pbr
xmake r usd-viewer --renderer=deferred --backend=Vulkan
```

The PocketJS interface provides a searchable scene tree, object inspector, visibility controls, file opening/reloading, camera controls, and live PBR/deferred switching. Without a path it opens the material studio sample with deferred rendering. Deferred mode also exposes color, normal and roughness views. Opening an invalid file displays an error and preserves the current scene.

Drag **LMB** to orbit, **MMB** to pan, scroll to zoom, press **F** to frame the selection (or scene), **Tab** to switch renderer, and **Esc** to clear the selection. Close the window to exit. `--backend=DX12` selects DirectX 12 on Windows; `--headless --frames=3` runs a rendering smoke test.

See the [viewer guide](examples/usd-viewer/README.md) for the code layout, supported USD features, and deterministic GPU E2E tests:

```powershell
xmake build usd-viewer-e2e
xmake r usd-viewer-e2e Vulkan
xmake r usd-viewer-e2e DX12
```

### Testing

```powershell
xmake build unit_tests
xmake r unit_tests --gtest_list_tests
xmake r unit_tests --gtest_filter="SuiteName.*"
xmake r unit_tests --jobs=8  # run in parallel shards; add --verbose to enable engine logs

xmake build asset_pipeline_tests
xmake r asset_pipeline_tests
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
