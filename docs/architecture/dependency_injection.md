# Plugins, dependency injection and ownership

`core::Runtime` owns process foundations; `App` assembles an independently owned World through plugins. Construct Runtime before Apps and their PMR-backed configuration, and destroy it after all Apps, assets and outstanding callbacks. This ordering also places PluginSet allocations inside the memory pool's lifetime.

![Runtime foundations outside Apps; plugins install resources and systems into each World.](../assets/di_engine_composition.svg)

## Responsibilities and dependency direction

| Layer | Responsibility |
| --- | --- |
| `core::Runtime` | Logging, MemoryManager, IOService, background JobSystem; initialized in that order and released in reverse |
| `core::PluginSet<Host>` | Generic one-time `Build(Host&) const` installation; no dependency on ECS, platform or engine |
| `ecs::World` | Entities, per-World resources, systems and validated schedules; depends on core |
| `App` | PluginSet, main World, caller-thread frame executor, event-loop callbacks and SubApps; depends on core and ECS |
| `gfx` / `render` plugins | Graphics ownership and render-World assembly within their functional modules |
| `engine` plugins | Default combination and composition connecting platform, assets, input, GUI and physics |

Plugins describe installation. Their configuration objects may be temporary; persistent state belongs to World resources and systems. There is no plugin tick, plugin-owned manager hierarchy, service singleton or implicit dependency discovery. Duplicate installation throws. Recursive cycles report the installation chain. A failed Build poisons the PluginSet; App refuses to execute partially installed state, which remains safely destructible. Install dependencies explicitly before consumers.

## Assembly and constructor injection

```cpp
import engine;

hitagi::core::Runtime runtime;
hitagi::App app(runtime);
app.AddPlugin(hitagi::DefaultPlugins{.runtime = runtime});
auto& platform = app.GetWorld().GetResource<hitagi::platform::PlatformHost>();
app.AddPlugin(hitagi::GuiPlugin{.platform = platform, .files = runtime.IO()});
app.Run();
```

Resource lookup belongs at this assembly boundary. `GuiPlugin` receives a PlatformHost and IOService explicitly; `AssetPlugin` receives IOService and JobSystem. Their runtime objects retain these narrow dependencies. A plugin never retrieves another plugin object to use it as a service. Application-specific plugins follow the same pattern; see `USDViewerPlugin` and the viewer entry point.

`DefaultPlugins` installs Time, Platform (including Input), Graphics, Asset and Debug. GUI and Physics are optional. Render policy remains explicit: import `render`, install RenderPlugin followed by the desired pipeline plugins, or compose a graph explicitly. `PhysicsPlugin` installs PhysicsWorld; the consumer explicitly calls Step.

`platform::PlatformHost` owns the native window, events, input and modal callback. PlatformPlugin connects it to App's event loop. A headless tool can construct Runtime and App without installing PlatformPlugin; use a bounded `Run(frame_count)` or provide an event loop. PlatformConfig contains window/surface settings. Graphics backend, asset root and log level belong to their respective plugin or Runtime configuration.

## Optional authoring plugins

The `editor` module exports `UsdImportPlugin` and `SceneImportService` through its
`:plugin` and `:scene_import` partitions. The plugin installs one service in the
host World; it creates no window, renderer, child World or recurring task.
Neither App nor DefaultPlugins depends on editor.

```cpp
import core;
import app;
import asset;
import editor;

hitagi::core::Runtime runtime;
hitagi::asset::AssetService assets(runtime.IO(), runtime.Jobs(), "assets");
hitagi::App app(runtime, "AssetTool");
app.AddPlugin(hitagi::UsdImportPlugin{.assets = assets, .jobs = runtime.Jobs()});
auto& importer = app.GetWorld().GetResource<hitagi::SceneImportService>();
hitagi::EditorCookContext document_sources;
auto scene = importer.Import("assets/test/test.usda", {.cook_context = &document_sources});
```

`UsdImportPolicy` owns the configured material getter/processor callbacks and
optional logger. The plugin copies this policy into the service, so temporary
plugin configuration can expire after installation. Captured references still
require their providers to outlive the service. Each App can install its own
policy; the framework selects no shading model. USDViewer supplies its PBR,
Toon and deferred material conversion from the example module.

Import is synchronous and preserves `ImportEditorScene` behavior: USD conversion
does not automatically register the scene; cooked loading goes through AssetService
and bypasses the USD material policy. Unknown formats and USD open failures return
null; parsing/loading exceptions propagate. `SceneImportOptions` supplies the
resource base path and an optional CookContext for that call only. CookContext
belongs to the document/import session and is never retained by the service;
USD asset paths remain resolved by USD, while cooked loading uses the supplied
base path. Callers serialize access to mutable callback state and document data.

`UsdSceneImporter`, `ImportEditorScene`, Cook functions and material compilation
remain directly usable by tools and tests. Cook and compilation stay explicit
operations chosen by the host.

## Four injection shapes

| Shape | Use | Example |
| --- | --- | --- |
| Constructor reference | Long-lived service | `AssetService(IOService&, JobSystem&, root)` |
| Operation argument | Capability needed for a call | `World::Update(jobs, delta_seconds)`, `Resource::Load(context)` |
| Function capability | A narrow operation | `ImageLoader`, `JobSubmitter` |
| Data value | An already-produced result | `render::ShaderSource` |

The composition root assembles `asset::ResourceLoadContext` from `gfx::GraphicsContext` device, queues, bindings and compiler references. This keeps gfx independent of asset. Low-level consumers receive these references, never GraphicsContext or App as a service locator. GraphicsContext preserves their native dependency order.

![Constructor, call, capability and value injection.](../assets/di_dependency_graph.svg)

`MakeFileImageLoader` captures IOService and a path; `JobSystem::MakeSubmitter` captures the executor. Texture decode runs on workers and GPU upload remains an explicit `Load(context)` operation. A task retaining a texture does not extend the lifetime of those borrowed services.

![Texture construction and asynchronous decode.](../assets/di_texture_decode.svg)

IOService currently implements synchronized file I/O and caching. Reads return owning Buffer snapshots so later reloads cannot invalidate a reader. Returned buffers still use the Runtime allocator and must be released before Runtime. Networking is not implemented; file paths retain filesystem semantics.

## World resources and scheduling

`EmplaceResource<T>(args...)` owns one T per World; `BorrowResource(T&)` explicitly borrows it. Duplicate types are rejected. Worlds have separate resource sets, and multiple Worlds and Apps can coexist. App SubApps use the same Runtime but independent Worlds; an explicit extraction callback transfers data before each SubApp update.

Tasks declare resource access using `ecs::Res<T>` and `ecs::ResMut<T>` alongside at most one Query. Schedule validates missing resources, writable aliasing and unordered write conflicts before executing anything. Captured references and cross-World aliases are not inferred: serialize them explicitly. Structural resource installation during World::Update is rejected. Resource replacement/removal and cross-World synchronization are not provided.

## Shutdown and lifetime

App waits for the shared Runtime job pool while resources remain alive, then shuts down SubApps and runs shutdown callbacks in reverse order. This conservative wait may include jobs from another App; per-App job scopes are not implemented. RenderPlugin drains GPU work and clears queued captures before render-World resources are released. gfx::GraphicsPlugin waits for GPU work before the parent World teardown; PlatformPlugin clears modal callbacks. World releases cached schedules and systems before resources, then destroys owned resources in reverse installation order. Borrowed resources remain the owner's responsibility.

Construct providers before consumers. Destroy external assets, snapshots and callbacks before the services and allocator they borrow. Runtime drains workers before releasing IO and memory; MemoryManager restores the previous PMR resource. System destruction must not launch new asynchronous work.

Standalone tests and tools can still explicitly construct individual services without App. Plugins are reusable assembly recipes, not a requirement for using the underlying APIs.

## Source entry points

- Foundations: [runtime.cpp](../../hitagi/engine/core/runtime.cpp), [plugin.cpp](../../hitagi/engine/core/plugin.cpp), [io_service.cpp](../../hitagi/engine/core/io_service.cpp)
- Assembly: [App](../../hitagi/engine/app/host.cpp), [built-in plugins](../../hitagi/engine/plugins.cpp), [viewer entry](../../examples/usd-viewer/main.cpp)
- Resources and execution: [resource.cpp](../../hitagi/engine/ecs/resource.cpp), [world.cpp](../../hitagi/engine/ecs/world.cpp), [schedule.cpp](../../hitagi/engine/ecs/schedule.cpp)
