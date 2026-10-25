# Dependency injection

Hitagi modules declare dependencies through constructor arguments, operation parameters, function objects, and data values. The caller supplies each dependency at the boundary where it is used.

`Engine` is the composition root for an engine instance: it creates runtime services, assembles the module tree, and owns their lifetimes. Tests and standalone tools assemble their own services according to the same lifetime rules.

## Four injection shapes

The injection shape follows how the consumer uses the dependency:

| Shape | When to use it | Examples |
| --- | --- | --- |
| Constructor injection (reference) | The object needs access to a service, or data backed by that service, throughout its lifetime | `AssetManager(FileIOManager&, JobSystem&)`, `PhysicsWorld(JobSystem&)`, `GuiManager(Application&, FileIOManager&)` |
| Call injection (parameter) | A particular operation needs a service the caller already has | `World::Update(JobSystem&)`, `Resource::Load(const ResourceLoadContext&)` |
| Capability injection (function object) | The consumer needs a specific operation supplied by a service | `ImageLoader` obtains pixels; `JobSubmitter` submits a task |
| Value injection (data) | The consumer needs the result produced by a service | `render::ShaderSource` carries source text and its path |

![Four injection shapes illustrated as objects and call scopes: AssetManager service references, Scene and World call parameters, Texture function capabilities, and ShaderSource values passed to render passes.](../assets/di_dependency_graph.svg)

### Constructor injection: services for an object's lifetime

`AssetManager` receives `FileIOManager&` and `JobSystem&` in its constructor and stores those references. Its owner creates the services first and destroys the manager before them:

```cpp
hitagi::core::FileIOManager file_io;
hitagi::core::JobSystem job_system;
hitagi::asset::AssetManager assets(file_io, job_system, "assets");
```

`PhysicsWorld` receives a `JobSystem&` for simulation work. `GuiManager` receives `Application&` for application and input access, and `FileIOManager&` to load fonts. Its font atlas retains pointers into the file cache, so the file service must outlive the GUI manager even though font loading occurs during construction.

### Call injection: an executor for one update

`ecs::World` and `asset::Scene` receive an executor for each update. The caller owns the job system:

```cpp
class World {
public:
    explicit World(std::string_view name);
    void Update(core::JobSystem& jobs);  // execution through the supplied executor
    // Other declarations omitted.
};
```

The update runs the validated dependency graph on that executor and waits for it to finish. Frame updates call `Update(engine.Jobs())`. Cooked loading and USD import take the same kind of reference for the evaluation that finishes the import. See [ECS contracts](ecs.md) for scheduling and structural-update rules.

GPU resource loading also receives services per operation:

```cpp
struct ResourceLoadContext {
    gfx::Device& device;
    gfx::CommandQueues& queues;
    gfx::BindlessUtils& bindings;
    const gfx::ShaderCompiler& shader_compiler;
};
```

`Resource::Load(const ResourceLoadContext&)` uses this context to create or upload GPU resources. `Engine::ResourceLoadContext()` assembles the references for application callers. The services must outlive the GPU resources created through them.

### Capability injection: texture loading and task submission

A lazily loaded texture receives functions for obtaining CPU pixels and submitting decode work:

```cpp
using ImageLoader = std::function<ImageData()>;

Texture(std::filesystem::path path,
        ImageLoader loader,
        std::string_view name = "",
        core::JobSubmitter decode_submitter = {});
```

`path` identifies the resource for deduplication and cooked references. `ImageLoader` supplies the pixels. `JobSubmitter` supplies asynchronous execution; an empty submitter selects synchronous decoding.

`AssetManager` creates these capabilities from its injected services:

```cpp
std::make_shared<Texture>(
    key,
    MakeFileImageLoader(m_FileIO, key),
    name,
    m_JobSystem.MakeSubmitter());
```

`MakeFileImageLoader` captures `FileIOManager&` and the path. When invoked, it reads the bytes and passes them to an image codec. The codec operates on buffers and image data; the loader owns the file-reading step.

On the asynchronous path, the submitted task holds a `shared_ptr<Texture>` and invokes `DecodeCPU` on a worker. Decoding produces CPU data; GPU upload occurs through a later `Load(context)` call. Asynchronous decoding requires both a submitter and shared ownership of the texture; otherwise decoding runs synchronously.

![Two stages of texture decoding: AssetManager assembles Texture with function capabilities, then a queued decode task invokes the loader on a worker.](../assets/di_texture_decode.svg)

### Value injection: shader source for a render pass

Render passes receive shader source as a value:

```cpp
struct ShaderSource {
    std::filesystem::path path;   // diagnostics and relative #include resolution
    std::pmr::string code;
};
```

The optional deferred renderer reads its sources with `render::deferred::LoadShaderSource(file_io, path)` during construction. It then passes `ShaderSource` values to `GBuffer`, `DeferredLighting`, and `GBufferDebugView`, alongside their device, binding, and compiler references.

The passes retain shader data and graphics dependencies. File access belongs to the assembly step, and `DeferredRenderer` does not retain `FileIOManager&` after construction. Editor viewport passes use the same value-injection pattern.

## Engine composition

`Engine` owns infrastructure services and a runtime ECS World. The host supplies
`AppConfig`, registers application systems, and chooses rendering policy.

![Engine members own facilities; World owns bound systems and their cached schedule.](../assets/di_engine_composition.svg)

Nested containers show ownership; amber labels identify borrowed dependencies.
Memory, file I/O, jobs, application, device, queues, bindings, compiler, assets and
physics are explicit Engine members. Services with no per-frame work do not
declare empty tasks. World owns system state and a cached Schedule, not a module
tree. Systems receive constructor-injected references and declare named work in
`ConfigureSchedule(Schedule&)`. Execution order belongs to host-defined groups and dependency edges.

Engine clears its modal callback, waits for CPU submissions and GPU completion,
then releases the World before its services. Queues and bindings outlive their
consumers and are released before Device. See [runtime scheduling](runtime_schedule.md)
for application frame groups, caller-thread execution, shutdown and host examples.

### Application access to services

Game and editor application code obtains services through the `Engine&` it holds:

| Use | Engine accessors |
| --- | --- |
| Infrastructure | `FileIO()`, `Jobs()` |
| Application and simulation | `App()`, `Physics()` |
| Graphics facilities | `Device()`, `Queues()`, `Bindings()`, `ShaderCompiler()` |
| Resources | `Assets()`, `ResourceLoadContext()` |

The application layer uses Engine to assemble workflows. Consumers receive the
specific dependencies they need. Hosts explicitly import `render` and register
concrete renderer systems through `engine.GetWorld().RegisterSystem<T>(args...)`.
They retain a borrowed concrete reference for frame submission. Custom systems
declare their own tasks without inheriting an engine base class. Engine has no
default renderer, renderer slot, or fixed GUI slot. See [Overall abstraction
design](overview.md) for engine, editor and game boundaries.

## Lifetime rules

A borrowed reference, including one captured by a function object, does not extend the referenced object's lifetime. The owner must keep services alive for all consumers and outstanding work.

* `Engine::~Engine()` destroys the runtime World before its infrastructure members are destroyed.
* Graphics consumers must be destroyed before bindings and queues; those facilities must be destroyed before the device.
* `MakeFileImageLoader` captures `FileIOManager&`, and `JobSystem::MakeSubmitter()` captures the job system. Both services must outlive textures holding those capabilities.
* A decode task's `shared_ptr<Texture>` keeps the texture alive while the task runs. It does not extend the lifetime of the injected file or job services.
* `JobSystem` waits for outstanding work during destruction. The file service remains alive until after that wait because it is declared before the job system.
* A parallel ECS schedule uses the supplied job system for that run and clears its temporary executor pointer when the run ends.

Resource registries in `AssetManager` hold weak references for lookup and deduplication. Actual ownership follows the resource graph and its consumers. Callers retaining resources outside the runtime World must ensure their injected services remain alive; destroying an engine does not invalidate or release an external `shared_ptr` automatically. See [Resource lifecycle](../assets/resource_lifecycle.md).

## Tests and standalone tools

The unit-test entry point owns the process-wide `MemoryManager` that installs the default PMR resource. Fixtures own the additional services their tests require, in dependency order:

```cpp
class AssetManagerTest : public ::testing::Test {
protected:
    hitagi::core::FileIOManager file_io;
    hitagi::core::JobSystem job_system;
    hitagi::asset::AssetManager assets{file_io, job_system, "assets"};
};
```

The fixture destroys `assets` first, followed by `job_system` and `file_io`. ECS and scene tests construct a `JobSystem` and pass it to `Update`. Render-pass tests can supply shader source values directly.

A standalone tool composes services in the same way. The PBR cook command uses this declaration order:

```cpp
hitagi::core::MemoryManager memory_manager;
hitagi::core::FileIOManager file_io;
hitagi::core::JobSystem job_system;
hitagi::asset::AssetManager asset_manager(file_io, job_system, "assets");
```

This assembly supports asset work without an interactive engine or editor instance.

## Source entry points

* Engine assembly and accessors: [engine.cpp](../../hitagi/engine/engine.cpp), [engine.cppm](../../hitagi/engine/engine.cppm)
* Runtime module ownership and tick order: [runtime_module.cpp](../../hitagi/engine/core/runtime_module.cpp)
* Asset service injection and texture capabilities: [asset_manager.cpp](../../hitagi/engine/asset/asset_manager.cpp), [texture.cpp](../../hitagi/engine/asset/texture.cpp)
* Executor capability: [job_system.cpp](../../hitagi/engine/core/job_system.cpp)
* ECS update and execution: [world.cpp](../../hitagi/engine/ecs/world.cpp), [schedule.cpp](../../hitagi/engine/ecs/schedule.cpp)
* Shader values and renderer assembly: [types.cpp](../../hitagi/engine/render/deferred/types.cpp), [renderer.cpp](../../hitagi/engine/render/deferred/renderer.cpp)
* Standalone cook composition: [asset pipeline tests](../../hitagi/editor/test/material_pipeline_test.cpp)
