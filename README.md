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
- **Render**: a deferred renderer (depth prepass, shadow maps, G-buffer, deferred lighting, debug views) plus GUI and text rendering
- **Asset**: runtime resources (textures, materials, meshes, cameras, lights, scenes) loaded from a cooked binary format, with GPU residency management in `AssetManager`
- **ECS**: archetype-based entities, filters, and system scheduling
- **Physics**: integration of [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
- **GUI and input**: [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) and an HID input manager
- **Platform**: Win32 on Windows, SDL3 elsewhere
- **Editor**: a generic authoring library with an asset browser, scene viewport, inspectors, OpenUSD import/export, and asset cooking

Platform status: Windows is the primary target and the only one covered by CI. Linux/Wayland code paths exist for Vulkan and SDL3 but are not regularly validated.

## Repository Layout

| Path | Contents |
| --- | --- |
| [hitagi/](hitagi) | Engine modules (`core`, `math`, `gfx`, `render`, `asset`, `ecs`, `physics`, `gui`, `hid`, `platform`, ...) and the editor library |
| [hitagi/interop/](hitagi/interop) | Module bridges for third-party dependencies (`interop.<library>`) |
| [examples/](examples) | Editor entry point, ImGui demo, PBR demo, and Snake game |
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

Other third-party dependencies (OpenUSD, ImGui, Jolt, Taskflow, DirectX Shader Compiler, the DirectX 12 Agility SDK, ...) are downloaded and built by XMake during configuration, so keep a network connection available. The first configuration builds OpenUSD and can take a long time.

### Configure and Build

```powershell
git clone https://github.com/L-Sun/HitagiEngine
cd HitagiEngine

# Use -m release for an optimized build; --toolchain=msvc is also supported.
xmake f -m debug --toolchain=clang-cl
xmake
```

The default build contains the engine and the editor. Other targets can be built explicitly or added to the default build with `--examples=y`.

| Option | Default | Description |
| --- | --- | --- |
| `--examples=y` | `n` | Include the game and GUI examples in the default build |
| `--profile=y` | `n` | Enable [Tracy](https://github.com/wolfpld/tracy) profiling |
| `--benchmarks=y` | `n` | Enable the `memory_benchmark` and `ecs_benchmark` targets |
| `--ispc=n` | `y` | Disable ISPC acceleration in the math library |

### Running

```powershell
xmake r editor               # editor
xmake r imgui-demo           # Dear ImGui demo
xmake r pbr-demo-game        # PBR demo; pbr-demo-game-editor opens it in the editor
xmake r snake-game           # Snake; snake-editor opens it in the editor
```

### Testing

```powershell
xmake build unit_tests
xmake r unit_tests --gtest_list_tests
xmake r unit_tests --gtest_filter="SuiteName.*"
xmake r unit_tests --jobs=8  # run in parallel shards; add --verbose to enable engine logs

xmake build editor_tests
xmake r editor_tests
```

## Documentation

- [Dependency injection](docs/architecture/dependency_injection.md)
- [Asset module](docs/assets/README.md): resource lifecycle, materials, and the cooked binary format
- [Interop boundaries](docs/development/interop.md)
- [Debugging on Windows](docs/development/debugging.md)
- [AGENT.md](AGENT.md): contribution conventions, architecture boundaries, and validation guidance

## License

See [LICENSE](LICENSE).
