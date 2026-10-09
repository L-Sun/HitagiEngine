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
- Component descriptions are registered once, never replaced, and outlive the
  archetypes. Each archetype column references its description and stores only
  its layout offset. Canonical sorted component-ID sequences identify archetypes;
  hash collisions are resolved by comparing the complete sequences.
- Archetype owns its chunks and both ID-to-row and dense row-to-ID metadata.
  Swap-removal updates both mappings. It does not read an Entity object to find
  the last row, so compaction does not depend on a component object's lifetime.
- Schedule receives EntityStorage, a logger and a debug name. Task invocation
  visits matching archetypes and their chunks directly using fixed-size arrays
  of requested columns, without materializing a component-by-buffer matrix.
  Filters are evaluated on every traversal; no matches or task buffer addresses
  are cached across updates.
- SystemManager retains its public name but acts as a registry. It stores
  already-bound lifecycle callbacks with no World parameter, plus update
  callbacks taking Schedule. World performs lifecycle binding at registration.
  Registry and scheduler implementations never call back into World directly.

The interface-partition import graph is acyclic. Each partition keeps its
implementations alongside its declarations. Schedule uses an indexed loop to
connect adjacent writers, avoiding the clang-cl/MSVC STL `zip_view` interface-BMI
compiler issue without a separate implementation unit.

## Storage and construction

- Static and dynamic component addition/removal share one migration operation.
  A newly added component is constructed before existing components are moved.
  If its constructor throws, the destination row is released and the source
  entity is unchanged. Failed CreateMany calls destroy all rows created by that
  batch; IDs consumed by failed batches are not reused.
- Allocation and initial construction may throw. Relocation (move, or copy
  fallback) and destruction retain the non-throwing contract required by dense
  swap-removal. Throwing from those callbacks terminates; this is not a general
  strong-exception guarantee for arbitrary component operations. A constructor
  must clean up its own partial work if it throws. Component construction,
  relocation and destruction callbacks must not reenter structural ECS operations.
- ComponentInfo has an optional trailing alignment field, defaulting to 1 for
  dynamic byte data; static components use alignof(T). Sizes must be nonzero and
  multiples of their power-of-two alignment. Conflicting re-registration of a
  dynamic name with a different size/alignment is rejected; existing callbacks
  are not replaced.
- Chunks normally target 2 KiB, with component columns aligned to at least 64
  bytes and respecting any larger component alignment. If even one row exceeds
  the target size, a larger single-row chunk replaces the zero-capacity layout.
- Dynamic components without an initial constructor are zero-filled. Without a
  move or copy callback they are treated as raw bytes during relocation.
  Nontrivial dynamic objects must supply construction, non-throwing relocation
  and destruction callbacks appropriate for their object lifetimes.

## Scheduling and lifecycle

- ParameterTraits is the single source for task parameter validation, component
  identity, access mode and argument binding. Dynamic parameter names must match
  the number of trailing dynamic pointers exactly. Conflicting access modes for
  the same component within one task are rejected; repeated identical accesses
  are deduplicated for ordering.
- Explicit order constraints are edges, not a single-successor map. Multiple
  successors and duplicate requests are supported. All implicit and explicit
  edges are deduplicated and cycle-checked before generating either serial
  order or Taskflow edges. Cycles throw invalid_argument before any task runs.
  Unknown task names retain their warning-and-ignore behavior.
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
Additional regression cases cover constructor/batch rollback, raw dynamic data
preservation, descriptor stability, large/aligned layouts, multi-successor and
cyclic graphs, parameter validation, mutable filters and newly created archetypes.
