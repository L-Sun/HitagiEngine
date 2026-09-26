<div align="center">
    <h1> Hitagi Engine</h1>
    <div>
        <a href="https://github.com/L-Sun/HitagiEngine/actions?query=workflow%3Awindows-build">
            <img alt="GitHub Workflow Status" src="https://img.shields.io/github/actions/workflow/status/L-Sun/HitagiEngine/windows.yaml?logo=windows&style=flat-square">
        </a>
    </div>
    <hr>
</div>

Hitagi Engine is an experimental game engine written in C++20.

## Architecture
- Core
    - A pmr-based memory allocator modeled on TCMalloc (the span implementation is missing)
    - A simple file I/O module
    - A simple thread pool
- Math
  - A math library for 3D operations, with swizzle support and ISPC acceleration (requires the [Intel SPMD Program Compiler](https://github.com/ispc/ispc))
    - Matrices are stored in row-major order on both the CPU and the GPU
- Asset management
    - Parses scenes with OpenUSD and imports them into AssetManager
- Graphics interface
    - Graphics API abstraction
    - DX12 backend (in progress)
    - Render Graph (in progress)
- HID
- GUI, based on Dear ImGui
- Platform layer
    - Windows


## Building
Only DX12 is supported for now, so this section covers building on Windows.

### Environment
Install Visual Studio's C++ development support first. Installing the Visual Studio IDE usually includes the C++ toolchain.
The steps below set up the toolchain without the IDE.

1. Install the [Visual Studio Installer](https://visualstudio.microsoft.com/downloads/), then select
    - MSVC v142 (the newest available, if possible)
    - C++ Clang-cl build tools, for Clang builds
    - C++ core features and the C++ build tools core features
    - Windows 10 SDK (the newest available)
    - Windows Universal C Runtime
2. Install XMake to build the project. See the [XMake installation guide](https://xmake.io/#/guide/installation).
3. Install [Clang](https://github.com/llvm/llvm-project/releases/download)

### Build steps
Clone this repository and enter the project directory
```bash
git clone https://github.com/L-Sun/HitagiEngine
cd HitagiEngine
```

Build with the command below. Dependencies may be downloaded during the build, so keep the network available
```bash
xmake f -m debug # switch to release if you prefer
```

#### Building with Clang
Install every dependency with MSVC first (some dependencies cannot be built with Clang), then run
```bash
xmake f -m debug -c # make sure dependencies are built with MSVC
xmake f -m debug --cc=clang --cxx=clang++ # switch the project itself to Clang
xmake
```

### Running
`examples` contains a few small demos. After the build finishes, start one with
```bash
xmake r playground
```
