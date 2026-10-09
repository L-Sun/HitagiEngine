# External dependency modules

Project code imports `interop.<library_name>` instead of including vendor headers.
Each bridge includes the upstream headers in its global module fragment and
exports the original entities with using-declarations. This retains upstream
types, templates and ABI; it does not compile a second copy of the library.
Vulkan-Hpp and magic_enum use their official modules through re-export bridges.

## Build ownership

All bridges live in `hitagi/interop`. Targets are separated only at product
boundaries; there is no target per package.

| Target | Consumers | Modules |
| --- | --- | --- |
| `hitagi_interop` | Runtime engine and games | `fmt`, `spdlog`, `taskflow`, `imgui`, `sdl`, `jolt`, `freetype`, `png`, `jpeg`, `range_v3`, `cxxopts`, `dxc`, `spirv_reflect`, `vulkan`, `vma`, `magic_enum`, `tracy`, `tracy.vulkan`; Windows: `win32`, `dx12`, `d3d12ma`, `tracy.dx12` |
| `hitagi_interop_editor` | Editor and game editors | `usd`, `nlohmann_json`, `imfilebrowser` |
| `hitagi_interop_test` | Unit/editor tests and benchmark test helpers | `gtest`, `gmock` |
| `hitagi_interop_benchmark` | Opt-in benchmarks (`--benchmarks=y`) | `benchmark` |

Every name in the table has the `interop.` prefix. USD and testing libraries
are not dependencies of the runtime target. Package requirements/configuration
are declared in the root configuration and interop build file, not in individual
consumers. Shared linking is requested only for the intended compiled libraries;
Windows DX12 packages are required only on Windows. Bridges never import
engine modules. The vendored file browser's unchanged header/implementation now
live beside its bridge; VMA's sole implementation also remains in this directory.

The default build includes the editor. `--examples=y` adds game/GUI examples to
that build; their targets remain available explicitly when the option is off.
Benchmark dependencies and targets are only loaded with `--benchmarks=y`:

```powershell
xmake f -m debug --toolchain=clang-cl --benchmarks=y
xmake build memory_benchmark ecs_benchmark
xmake r memory_benchmark --benchmark_list_tests=true
```

Benchmark executables are never part of the default build or the unit-test runner.

The explicit export lists cover the APIs used by this repository, not every
possible upstream API. Extend the owning bridge when using another symbol;
do not restore a direct vendor include. Libraries do not implicitly re-export
unrelated libraries: for example, fmt users import `interop.fmt` explicitly.
Transitive package internals such as zlib/TBB remain implementation dependencies
of their upstream libraries; the engine does not consume their APIs directly.

## Macros and configuration

- Constants and ordinary function-like macros become typed constants/functions
  at the bridge. Win32 A/W aliases are resolved explicitly at call sites.
- `tracy_macros.hpp` retains lexical scope, source location, and disabled
  argument elision. Test registration/assertion macros and COM type deduction
  also stay in thin headers without vendor includes.
  DXC's non-Windows adapter follows upstream
  [WinAdapter.h](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/WinAdapter.h)
  for UUID deduction and single evaluation of the output pointer.
- `jolt_macros.hpp` matches upstream's `JPH_NO_DEBUG`, `NDEBUG`, `JPH_DEBUG` and
  `JPH_ENABLE_ASSERTS` conditions. Jolt build definitions remain public.
- libpng's adapter returns the upstream jump buffer; **setjmp still executes
  in the caller**, never inside a forwarding function.
- GoogleTest follows the package repository's latest version without a version pin.
  `gtest_macros.hpp` contains the call-site macro dependency closure adapted from
  GoogleTest 1.17.0, with its license. It requires the project's normal
  exceptions/RTTI configuration. If an upgrade breaks compatibility, update the
  adapter against upstream; also extend it when a new macro is needed.
  GMock currently exports matcher/mock types; no project caller uses MOCK_METHOD.
- Standard library headers needed for macros/global C names or the current
  clang-cl/MSVC-STL module visibility limitations remain textual. They are not
  third-party bridges. Generated ISPC declarations also retain their C ABI header.

## Verification

Run the include-boundary audit from the repository root:

```powershell
xmake lua xmake/scripts/check_interop.lua
```

It includes excluded historical benchmark/test-main sources in its scan and
recognizes shader includes embedded in strings. This is an include audit, not
a compiler or clang-tidy pass. Build and run both test targets, build all game
and editor examples, and validate both `--profile=n` and `--profile=y` when
changing shared dependency definitions. Current validation is Windows/clang-cl;
Linux and other compilers require their own build checks.
