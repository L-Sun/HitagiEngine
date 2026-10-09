# External dependency modules

Project code imports `interop.<library_name>` instead of including vendor headers.
Each bridge includes the upstream headers in its global module fragment and
exports the original entities with using-declarations. This retains upstream
types, templates and ABI; it does not compile a second copy of the library.
Vulkan-Hpp and magic_enum use their official modules through re-export bridges.

Taskflow retains its native `tf::` types and APIs through `interop.taskflow`.
The exported namespace alias `hitagi::interop::tf = ::tf` is the preferred spelling:
engine code inside `hitagi` uses `interop::tf::Taskflow` and `interop::tf::Executor`.
Outside `hitagi`, use the full name or a local `namespace interop = hitagi::interop;`
alias. These are the same upstream types, not wrappers; the alias does not export
additional upstream names beyond the bridge's export list. On MSVC 14.51 the bridge
explicitly instantiates `std::vector<tf::Node*>` while Node is complete, avoiding
recursive Graph/Node import errors. The local Taskflow 4.1.0 recipe's `modules`
configuration changes only two profiler helpers from `static inline` to `inline`,
so their definitions survive module import. Public classes, layouts and task
execution behavior remain unchanged. The version is pinned so these compatibility
fixes can be retested when upgrading. Extend the bridge's export list when using
additional upstream names; do not replace native types with look-alike wrappers.

For MSVC, `usd_count_compat.hpp` declares USD's reference-count overloads before
the upstream template definition and gives the bridge's count-tag constants
external linkage. This works around imported-template lookup and missing-tag
link errors without modifying installed USD headers or reference-count behavior.

## Build ownership

All bridges live in `hitagi/interop`. Targets are separated only at product
boundaries; there is no target per package.

| Target | Consumers | Modules |
| --- | --- | --- |
| `hitagi_interop` | Runtime engine and games | `fmt`, `spdlog`, `taskflow`, `imgui`, `sdl`, `jolt`, `freetype`, `png`, `jpeg`, `dxc`, `vulkan`, `vma`, `magic_enum`, `tracy`, `tracy.vulkan`; Windows: `win32`, `dx12`, `d3d12ma`, `tracy.dx12` |
| `hitagi_interop_editor` | Editor and game editors | `usd`, `nlohmann_json`, `imfilebrowser` |
| `hitagi_interop_test` | Unit/editor tests and benchmark test helpers | `gtest` |
| `hitagi_interop_benchmark` | Opt-in benchmarks (`--benchmarks=y`) | `benchmark` |

Every name in the table has the `interop.` prefix. USD and testing libraries
are not dependencies of the runtime target. Package requirements/configuration
are centralized in `hitagi/interop/xmake.lua`, alongside the bridge targets.
Consumers inherit dependencies through these targets. Shared linking is requested
only for the intended compiled libraries;
Windows DX12 packages are required only on Windows. Bridges never import
engine modules. The vendored file browser's unchanged header/implementation now
live beside its bridge; VMA's sole implementation also remains in this directory.

fmt and spdlog use static compiled libraries, with spdlog's external fmt dependency
matching the direct fmt configuration. This avoids MSVC module compilation errors
in spdlog's header-only implementation while retaining the separate `interop.fmt`
and `interop.spdlog` interfaces.

Only module interfaces and interface partitions are public sources. The engine
also recognizes `export module` declarations in `.cpp` files, since several
partitions use that extension. Ordinary implementation units stay private to
their static library; marking them public makes XMake propagate them to consumers
without the module references they require.

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
  at the bridge, after its named module declaration. The global module fragment
  contains only preprocessing directives; vendor declarations enter through
  includes. Capture macro values before undefining names reused by exported
  constants. Win32 A/W aliases are resolved explicitly at call sites.
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
  GoogleMock is disabled in the package configuration; project tests use GoogleTest only.
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
changing shared dependency definitions. The default Windows debug build has also
been validated with MSVC 14.51. Linux, profiling-enabled configurations and other
compilers require their own build checks for these compatibility changes.
