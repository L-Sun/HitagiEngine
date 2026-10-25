# ECS-driven runtime

Runtime owns process foundations; App assembles resources and systems through
plugins; Schedule orders and executes their work. C++ modules remain source-code boundaries and are unrelated to this runtime
execution model.

## Ownership and assembly

![Runtime owns foundations; each App owns its World, resources and systems.](../assets/di_engine_composition.svg)

`core::Runtime` owns logging, memory, IO and background jobs. App owns a plugin
registry, World and caller-thread frame executor. Optional engine plugins install
platform, graphics, asset, input, debug, GUI and physics resources. Plugins build
once; systems declare recurring work. See [ownership and DI](dependency_injection.md).

`App::GetWorld()` exposes the runtime World. A system is an ordinary type with
optional lifecycle hooks and `ConfigureSchedule(Schedule&)`. Constructor arguments
inject dependencies. Repeated registration returns and enables the same instance.

```cpp
import engine;
import render;

hitagi::core::Runtime runtime;
hitagi::App app(runtime);
app.AddPlugin(hitagi::DefaultPlugins{.runtime = runtime});
auto& world = app.GetWorld();
auto& graphics = world.GetResource<hitagi::gfx::GraphicsContext>();
app.AddPlugin(hitagi::render::RenderPlugin{
    .device = *graphics.device, .queues = graphics.queues, .bindings = *graphics.bindings,
    .window = &world.GetResource<hitagi::platform::PlatformHost>(),
});
app.AddPlugin(hitagi::render::pbr::PbrPlugin{.resources = {
    .device = *graphics.device, .queues = graphics.queues,
    .bindings = *graphics.bindings, .shader_compiler = graphics.compiler,
}});
auto& render_world = world.GetResource<hitagi::render::RenderApp>().app.GetWorld();
auto& renderer = render_world.GetResource<hitagi::render::pbr::PbrRenderer>();
auto& rendering = render_world.GetResource<hitagi::render::RenderContext>();
rendering.SetFrame(renderer.Capture({})); // replace with camera, draw items and lights
app.Tick();
```

Pipeline references are borrowed from the render World's resources. The child
App executes after its parent World; explicit extraction can transfer state
before it runs. Dependencies must outlive both the tasks and queued frame builders.
See [render ownership](overview.md#current-render-architecture).

## Work and execution

![One dependency graph drives caller-thread frame work and parallel scene work.](../assets/runtime_schedule.svg)

`ConfigureSchedule` declares tasks when the schedule is built, not on every frame.
Registry changes invalidate the cached schedule for the next update. Configuration
only declares work; frame-dependent data is read inside the task callbacks. A
zero-argument Request runs once per update even if the World has no entities;
Query callbacks also run once, and explicitly invoke Each or ParallelEach.

Group labels are enum values supplied by the host. ECS stores their exact enum
identity (`std::type_index`) and underlying value; different enum types never
become equal just because their integer values or hashes match. Aliases with the
same value in one enum intentionally identify the same group. Numeric enum order
has no scheduling meaning. Tasks without a group have no implicit frame phase.

The `app` module defines `FrameStage` and `ConfigureFrameStages`. App constructs
its World with First -> PreUpdate -> Update -> PostUpdate -> Render -> Last edges.
The `platform` module owns windows and native events; core and ECS know no frame
policy. Standalone Worlds can supply their own groups and ordering.

`World(name, configure_schedule)` retains an optional configuration callback.
On each schedule rebuild it calls this callback before configuring registered
systems. The unchanged schedule is cached. If configuration throws, the candidate
is discarded and the next Update retries; no candidate tasks have run. Callback
captures must outlive the World. Changing external configuration does not itself
invalidate a cached schedule: this hook is for stable composition policy.

All tasks in the first group finish before any task in the next group starts.
Empty groups preserve transitive constraints and participate in cycle validation.
A task currently belongs to at most one group. Group entry/exit nodes represent
barriers in the same graph as task dependencies, avoiding all-to-all task edges.

Query describes component access; Res<T> and ResMut<T> describe World resource
reads and writes. These accesses do not choose temporal order.
Task/group SetOrder edges define execution order. Unordered read/write or
write/write pairs are diagnosed before execution; read/read pairs can run
concurrently. Cross-group conflicts follow the same rule. Cycles, duplicate task
names and missing order endpoints also fail during graph preparation. See
[Query and explicit order](ecs.md#query-and-explicit-order).

```cpp
struct MySystem {
    Simulation& simulation;
    Renderer& renderer;

    void ConfigureSchedule(hitagi::ecs::Schedule& schedule) {
        schedule.Request("Game.Simulate", [this, &schedule] {
            simulation.Update(schedule.GetDeltaSeconds());
        }, hitagi::FrameStage::Update);
        schedule.Request("Game.Render", [this] {
            renderer.Draw(simulation);
        }, hitagi::FrameStage::Render);
    }
};
```

This example uses application-defined Simulation and Renderer types and assumes
the host constructed its World with `ConfigureFrameStages` (App does this). Captured
services are not inferred as ECS component accesses: express their ordering
explicitly. Independent tasks in one group have no promised relative order.

`World::Update(job_system, delta_seconds)` is the only update entry. Schedule
passes its validated core JobGraph to the supplied JobSystem and waits for
completion. App owns a CallingThread JobSystem for the frame World; Runtime owns
the shared thread-pool JobSystem for CPU services and scene updates. A one-worker
pool is still a worker thread, not a main-thread executor. No implicit
worker-to-main-thread migration is implemented. JobSystem also executes nested
Query batches, with cooperative waiting on its own workers.

`gfx::GraphicsFrameSystem` is exported directly by `gfx`. It borrows Device and
CommandQueues, accepts an `ecs::Group` from its host, and declares Device.BeginFrame
before Queues.NewFrame within that group. GraphicsPlugin places it in
First. The host calls `Device::EndFrame()` on the same thread after the whole frame,
including exceptional exits. App's frame guard supplies this pairing; EndFrame
is safe when schedule preparation failed before BeginFrame. It releases CPU-side
frame state without waiting for GPU completion. Metal owns its per-frame
autorelease pool behind these device methods; App contains no Apple pool code.
Standalone hosts using GraphicsFrameSystem must supply the same cleanup guard.
The gfx and render modules do not import engine. Their plugin partitions import
app to use its assembly and frame-stage policy. RenderPlugin registers one
Render.Execute task in its child World; concrete renderer plugins install the
resources used by frame builders. USDViewer declares input, scene update and
frame composition selection in the main World, then publishes after rendering
in the child World's Last group. Its scene and GUI passes share one graph.
Standalone callers may construct RenderContext and renderer utilities directly.

## Event loop and shutdown

App::Tick invokes the poll callback installed by PlatformPlugin, then runs the
frame World. A native modal resize callback runs the frame World directly,
without re-entering the native event pump. Frame guards end the device frame and
reset reentry state on exceptions. On macOS, Metal's frame pool nests inside the
Cocoa callback's native pool and drains before that callback returns. Device
initialization, frame-external operations and other threads have separate native
object lifetimes; the frame pool does not cover them.
Clock advancement stays at frame end, preserving the previous delta-time contract.
This change does not alter event-loop pacing or introduce reactive rendering.

App waits for Runtime jobs while its resources are alive, shuts down SubApps,
and invokes shutdown callbacks in reverse installation order. AssetPlugin drains
asset work, gfx::GraphicsPlugin waits for queues, and PlatformPlugin clears the modal
callback. RenderPlugin also drains its queues and releases graph/builder captures
before the child World destroys renderer resources. World is then released. System destructors must not launch asynchronous
work. External producers and resources remain their owner's responsibility.

World releases its cached schedule and shuts down systems in reverse registration
order while entity storage still exists. Bound system instances are retained by
each cached schedule, so unregistering during execution cannot destroy the
object used by later tasks in that snapshot. Lifecycle callbacks still run
immediately, as in the existing ECS contract; they must not invalidate data used
by active tasks. Registry mutation and world updates are not concurrently safe.

PhysicsWorld separately drains its own submitted Jolt jobs before destroying its
simulation and job adapter. A Jolt simulation barrier can complete before the
executor releases its final Job reference; waiting only on the simulation barrier
does not protect adapter lifetime. This local drain does not wait for unrelated
JobSystem work.

## Verification entry points

- `PluginTest`, `AppTest`, `ResourceTest` and `RuntimeTest`: installation errors,
  independent Worlds, resource conflicts, exception cleanup and shutdown lifetime.
- `RuntimeScheduleTest`: zero-entity tasks, caller-thread execution, group/order
  validation, cached state, unregister during execution, frame duration, and
  registration failure.
- `EcsTest`: existing component queries, mutation and serial/parallel contracts.
- `RendererSystemTest`, built-in renderer tests and `WindowRendererTest`: scene
  ordering, lifetime, GPU snapshots, minimized windows and modal resize frames.
- `usd-viewer-e2e`: the actual viewer plugins and main/render Worlds run for both replay
  and interactive execution; state and PNG output are compared across replay.
