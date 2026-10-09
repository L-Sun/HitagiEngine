# ECS ownership and update contracts

## Public usage

```cpp
ecs::World world("game");
world.RegisterSystem<MovementSystem, TransformSystem>();

ecs::Entity entity = world.GetEntityManager().Create();
entity.Emplace<Transform>();
Transform& transform = entity.Get<Transform>();

world.Update();            // serial
world.Update(job_system);  // parallel, waits for completion

world.GetSystemManager().Disable<MovementSystem>();
world.GetSystemManager().Enable<MovementSystem>();
world.GetSystemManager().Unregister<MovementSystem>();
```

Registration moved from `GetSystemManager().Register<T>()` to
`World::RegisterSystem<T>()`. Entity access, bulk creation, dynamic components,
filters, `Schedule::Request()` chaining and `Schedule::SetOrder()` retain their
existing syntax. System lifecycle hooks still receive `World&`; `OnUpdate`
receives `Schedule&` and declares tasks. Schedule is now constructed by World
and no longer exposes a `world` reference. Capture required dependencies when
binding system state rather than resolving them through the scheduler.

## Ownership and dependency boundaries

- World owns the entity manager, system registry and cached schedule.
- EntityManager owns the internal EntityStorage and constructs Entity handles.
  Handles contain a non-owning storage pointer and an ID. They must not outlive
  their World. A manager rejects handles belonging to another storage.
- EntityStorage owns component metadata, archetypes and entity locations. It
  does not depend on Entity, EntityManager or World. EntityManager registers the
  Entity component using the same metadata path as other static components.
- Archetype owns its chunks and both ID-to-row and dense row-to-ID metadata.
  Swap-removal updates both mappings. It does not read an Entity object to find
  the last row, so compaction does not depend on a component object's lifetime.
- Schedule receives EntityStorage, a logger and a debug name. Task invocation
  queries storage directly. No task buffer addresses are cached across updates.
- SystemManager retains its public name but acts as a registry. It stores
  already-bound lifecycle callbacks with no World parameter, plus update
  callbacks taking Schedule. World performs lifecycle binding at registration.
  Registry and scheduler implementations never call back into World directly.

The interface-partition import graph is acyclic. Each partition keeps its
implementations alongside its declarations. Schedule uses an indexed loop to
connect adjacent writers, avoiding the clang-cl/MSVC STL `zip_view` interface-BMI
compiler issue without a separate implementation unit.

## Scheduling and lifecycle

- Registering a system calls OnCreate once and enables it. Registering an
  existing system enables it without recreating it. Repeated enable, disable
  and unregister operations do not repeat their lifecycle callbacks.
- Enabling/disabling an unknown system is a no-op. Register it through World
  before changing its state.
- Registry changes increment a revision. World rebuilds its cached Schedule
  before the next Update when that revision changes. Unchanged systems do not
  redeclare their tasks every frame.
- Schedule construction snapshots enabled update callbacks. Changes made while
  collecting or executing tasks take effect in the next Update. Disabling a
  system does not cancel tasks already in the current snapshot.
- Lifecycle callbacks still run immediately when the registry is changed.
  Old task captures remain alive until the schedule is replaced at the next
  Update or the World is destroyed. They are not released by an immediate
  reset of an actively executing schedule.
- Registry mutation is not thread-safe. Do not concurrently register, enable,
  disable or unregister systems from multiple tasks or threads. Structural
  entity changes must not invalidate component buffers in executing tasks;
  defer them until the update completes. Lifecycle callbacks must respect the
  same restriction if invoked during task execution.
- Recursive World::Update calls are rejected. Exception paths restore the
  update guard so a later update remains possible.
- World shuts down systems while its entity storage and services are still
  alive, then releases its schedule. Shutdown callbacks must not throw.
  New registrations during shutdown are ignored.

These contracts are covered by ECS tests for lifecycle binding, cached-schedule
rebuilding, mutation during schedule collection/execution, shutdown access,
cross-world handles, cross-chunk compaction and dynamic-component migration.
