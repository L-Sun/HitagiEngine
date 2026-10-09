# AGENT.md

Guidance for coding agents working in this repository. Read the applicable module guidance as well; gfx has its own [AGENT.md](hitagi/gfx/AGENT.md).

## Collaboration

- Communicate in Chinese. Explain from concrete code and current behavior toward design choices, step by step, as a senior developer working with a junior developer.
- Evaluate the user's hypotheses independently. Explain counterexamples, assumptions, and tradeoffs when the evidence disagrees; do not merely agree.
- In design examples, spell out types where they matter and distinguish existing APIs from proposed ones. Do not rely on IDE inference or syntax highlighting.
- For design discussions, converge on a concrete change before implementing. For an authorized implementation, complete the relevant callers and verification.

## Commits

- Follow the repository's existing format: `emoji [scope] English description`. Inspect recent history for the appropriate emoji and scope; describe the concrete change.
- Group changes by responsibility/scope rather than by editing order. Closely related documentation changes can share one `docs` commit; separate unrelated scopes.
- Intermediate commits need not each compile independently unless the task requires it. Prefer coherent scope boundaries and validate the final combined state.
- Stage only the changes belonging to the requested work. Review the staged diff and check it for whitespace errors before committing.
- Preserve the configured signing policy. If signing needs a key unlock, report it rather than disabling signing. Report the resulting commit IDs and working-tree state; push only when requested.

## Coding Principles

These are defaults for engineering judgment; retain the invariants and correctness guarantees the task requires.

- Read the relevant declarations, implementations, and actual callers before changing the model. Establish responsibility, ownership, and lifetime first.
- Prefer semantic simplicity: fewer concepts, states, branches, and usage steps. Moving code into a helper alone does not reduce complexity; avoid tricks based on incidental enum values or bit layouts.
- For a logical simplification, establish its valid inputs and show equivalence through reasoning or proportionate checks. Do not trade clarity or correctness for fewer lines.
- Reuse existing concepts. Add a type, wrapper, overload, or abstraction only when it supplies missing semantics, enforces an invariant, or meaningfully reduces total complexity.
- Keep behavior with its semantic owner and local details near their use. A single-use computation often belongs in the caller, with a local lambda when useful; simple expressions such as `sizeof(T)` need no alias unless the name adds meaning.
- Extract helpers for a clear contract, substantial readability benefit, or meaningful reuse. Do not mechanically eliminate every repeated expression or short error branch.
- Let each layer expose its own concepts while reusing suitable lower-level descriptions and operations internally. Avoid duplicate descriptor models and forwarding APIs without a responsibility of their own.
- Prefer a small, explicit set of creation and access paths. Avoid convenience APIs that silently allocate resources or introduce extra ownership. Use access control and types to guide correct use.
- Unify names through type-safe overloads when the operation has the same meaning. Preserve distinctions between different operations; return useful results rather than a builder solely to enable chaining.
- Before adding machinery, check whether a simpler data layout, binding strategy, or usage pattern removes the need. A coherent simplification may require changing multiple layers rather than minimizing the diff.
- Introduce concurrency, extension points, fallback paths, and generic frameworks for actual requirements or an explicit contract. Consider future constraints without implementing speculative support. Do not remove necessary synchronization or validation just to shorten code.
- Use short names when context is unambiguous (`builder`); distinguish multiple objects by purpose (`producer_builder`, `consumer_builder`), not arbitrary numeric suffixes.
- **Do not use anonymous namespaces** (`namespace { ... }`) in project C++ code, including examples and tests. Keep helpers local or in the owning named namespace; keep implementation details unexported. Existing violations are not precedent.

## Build and Validation

Use XMake from the repository root. Preserve the active build configuration unless the task requires changing it. Windows uses clang-cl, UTF-8 sources, and MDd/MD runtimes for debug/non-debug modes.

```powershell
# Configure when needed; release uses -m release.
xmake f -m debug --toolchain=clang-cl

# Build and run a specific application.
xmake build editor
xmake r editor

# Build and inspect engine tests.
xmake build unit_tests
xmake r unit_tests --gtest_list_tests
# Replace SuiteName with a suite from the list above.
xmake r unit_tests --gtest_filter="SuiteName.*"

# Optional multi-process sharding for unit_tests.
xmake r unit_tests --jobs=8

# Editor tests are a separate target.
xmake build editor_tests
xmake r editor_tests
```

Other application targets include `pbr-demo-game`, `pbr-demo-game-editor`, `snake-game`, `snake-editor`, and `imgui-demo`. They can always be built explicitly; `--examples=y` includes them in the default build, while `editor` remains enabled by default. Optional configuration flags include `--profile=y` (Tracy) and `--ispc=n`. Target definitions live in [hitagi/xmake.lua](hitagi/xmake.lua), [hitagi/editor/xmake.lua](hitagi/editor/xmake.lua), and the example directories.

- Engine tests share `unit_tests`; select cases with GoogleTest filters, not historical per-module test targets. Both test targets are excluded from the default build. Benchmark sources are excluded from `unit_tests`; enable `--benchmarks=y`, then explicitly build/run `memory_benchmark` or `ecs_benchmark`. Benchmark packages and the separate `hitagi_interop_benchmark` bridge are disabled otherwise.
- The [unit test runner](xmake/scripts/run_unit_tests.lua) supports `--jobs=N` and forwards GoogleTest arguments. The default is one process; use `--verbose` to enable engine logs. These custom options do not apply to `editor_tests`.
- Use compiler/static-analysis diagnostics for problems the tools can locate. For standalone clang-tidy, use the compilation database and explicit checks; do not assume it reads the diagnostic configuration in [.clangd](.clangd).
- A full source scan must cover project `.cpp` and `.cppm` files, accounting for excluded files and missing compilation commands. Report checked, skipped, and failed files separately; importing a module is not the same as checking it as an entry.
- Run checks appropriate to the behavior changed. Add tests for meaningful behavior, boundary cases, or regressions; keep their maintenance cost proportional to the risk. Do not add tests that merely mirror trivial implementation details.
- Report build, test, and static-analysis results separately, with their actual scope and limitations. A successful build does not establish a clean clang-tidy scan.
- Save generated test reports, logs, and diagnostic artifacts under `temp/`. Windows LLDB troubleshooting is documented in [docs/development/debugging.md](docs/development/debugging.md).

## Architecture and Ownership

### Product Boundaries

These are intended ownership/dependency boundaries, not a claim that all current namespace names have been migrated.

| Scope | Responsibility | Project dependencies |
| --- | --- | --- |
| engine | Runtime foundations, gfx/render primitives, ECS, physics, GUI, runtime assets | No editor or game dependency |
| editor | Generic authoring, inspectors, viewports, USD import/export, cook tools | engine |
| game | Gameplay, game assets, rendering composition and material/pass policy | engine |
| game-editor | Integration of generic editor tools with game-specific rules | engine, editor, game |

Runtime packaging should require only engine, game, and cooked assets. USD authoring data, editable document graphs, and save-back workflows belong to editor/game-editor. Game-specific render pipelines and material compiler policies belong to game; engine render code provides reusable primitives and explicitly owned defaults.

Current engine code generally uses `hitagi::`; some editor and example types still use that namespace too. Follow local declarations when editing existing code and keep namespace migration separate from unrelated changes.

### Runtime Contracts

- Dependencies are explicit: inject long-lived services through constructors, per-operation executors through arguments, narrow capabilities as functions, and plain data as values. Do not reintroduce a global service registry.
- `Engine` is the composition root. It owns infrastructure services and the runtime module tree; injected dependencies must outlive their consumers. Tests own the services they need. See [dependency injection](docs/architecture/dependency_injection.md).
- The host supplies `AppConfig` and owns configuration persistence. `Engine::Tick()` advances the runtime modules. Use the real [editor entry point](examples/editor/main.cpp) or [GUI example](examples/imgui_demo/main.cpp) as an integration reference.
- `GuiManager` owns ImGui and produces `GuiDrawData`. `RenderRuntime::RenderGui()` consumes that data; `RenderRuntime` owns the graph/swapchain and compiles, executes, and presents. Rendering code must not acquire GUI data through hidden access to ImGui or a `GuiManager`.
- `IRenderer` builds rendering work through `RenderContext`; the current `DefaultRenderer` aliases `DeferredRenderer`. Runtime assets use `Load(const ResourceLoadContext&)` / `Unload()`.
- Matrices use row-major layout on CPU and GPU. DX12 is Windows-only; the Vulkan/platform code also has Linux/Wayland paths. Do not claim platform validation without running it.

### Modules and Source Navigation

The project uses C++23 modules. Determine a file's role from its module declaration: interfaces commonly use `.cppm`, but asset also has interface partitions declared in `.cpp`. Keep third-party includes at module boundaries using the global module fragment where needed.

The `engine` target collects sources through globs in [hitagi/xmake.lua](hitagi/xmake.lua), with explicit exclusions for interop, editor, tests, and platform-specific files. External dependency module bridges live in [hitagi/interop](hitagi/interop/xmake.lua), owned by independent interop targets. Runtime bridges come from `hitagi_interop` through the engine's public dependency; editor-only USD/JSON/file-browser bridges come from `hitagi_interop_editor`, and test libraries come from `hitagi_interop_test`. Do not pull authoring or test libraries into the runtime. Keep these bridges independent of engine modules; prefer upstream modules when available. For a new module, first determine its owning target and inspect those globs/exclusions. Add a separate target only for a distinct build/dependency boundary; export through the appropriate aggregate when needed.

Prefer consuming third-party dependencies through `interop` modules throughout the project. Reuse official modules where available; otherwise export the existing library entities through a global-module-fragment bridge without changing ABI or duplicating implementations. Extend an existing bridge instead of adding direct includes to consumers. Keep macros textual only when their call-site location, scope, declaration, or disabled argument-elision semantics require it; use a thin adapter header, not a header that merely includes the original heavy dependency. Tracy follows this pattern with `interop.tracy`, its GPU modules, and [tracy_macros.hpp](hitagi/interop/tracy_macros.hpp). Validate profiling-enabled and profiling-disabled builds when changing this boundary. All current direct third-party API dependencies are migrated, including examples/tests; transitive package internals are not engine APIs. Run `xmake lua xmake/scripts/check_interop.lua` to audit consumers, including excluded sources. See [interop boundaries](docs/development/interop.md) for macro contracts and validation.

Name dependency modules `interop.<library_name>`, with backend extensions under that library (for example, `interop.tracy.vulkan`). Keep independent libraries separate: import `interop.fmt` when naming `fmt::` entities and `interop.spdlog` when naming `spdlog::` entities; spdlog does not re-export fmt. Use the `interop.vulkan` and `interop.magic_enum` re-export entry points for their official upstream modules. Use the runtime/editor/test aggregate that owns the dependency boundary, not one target per package.

Keep a file's `import interop.*` declarations together, separate from standard-library and engine-module imports. Preserve platform guards around platform-specific imports.

| Area | Entry point |
| --- | --- |
| Runtime composition and services | [engine.cppm](hitagi/engine/engine.cppm), [core.cppm](hitagi/core/core.cppm) |
| Graphics and render graph | [gfx guidance](hitagi/gfx/AGENT.md) |
| Render interfaces and execution | [render.cppm](hitagi/render/render.cppm), [render_runtime.cpp](hitagi/render/render_runtime.cpp) |
| Runtime assets and residency | [asset documentation](docs/assets/README.md), [asset.cppm](hitagi/asset/asset.cppm) |
| ECS and scheduling | [ecs.cppm](hitagi/ecs/ecs.cppm) |
| Application and GUI | [app.cppm](hitagi/platform/app.cppm), [gui_manager.cppm](hitagi/gui/gui_manager.cppm) |
| Editor library | [editor.cppm](hitagi/editor/editor.cppm), [build target](hitagi/editor/xmake.lua) |

## Maintaining This Guidance

Keep these files focused on decisions, contracts, working commands, and a few source entry points. Update affected guidance when a change alters them. Link to source or focused documentation instead of duplicating class inventories, enum mappings, or example implementations. Distinguish current behavior, intended architecture, and known limitations.
