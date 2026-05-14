# JobSystem TODO

## Goal

Create one engine-owned CPU job boundary for ECS, render command recording, physics jobs, and asset streaming. Taskflow can remain the first backend, but engine modules should depend on `core::JobSystem` rather than directly owning their own executors or thread pools.

## Current State

- `core::JobSystem` is the engine-owned CPU job module.
- ECS schedules execute through `core::JobSystem`; `tf::Taskflow` remains internal to `ecs::Schedule`.
- `RenderGraph` records command contexts through `core::JobSystem` within deterministic execution layers.
- `PhysicsWorld` uses a Jolt `JobSystem` adapter backed by `core::JobSystem`.
- `AssetManager` exposes future-based async import jobs backed by `core::JobSystem`.

## Phase 1: Core Boundary

- [x] Rename the engine-facing thread module to `core::JobSystem`.
- [x] Use Taskflow as the initial `JobSystem` backend.
- [x] Replace the old thread module API directly; no compatibility alias is kept.
- [x] Add `Submit`, `RunTaskflow`, `WaitForAll`, worker count, and worker id accessors.
- [x] Create `JobSystem` from `Engine` before gameplay/render modules.

## Phase 2: ECS Integration

- [x] Remove `tf::Executor` ownership from `ecs::World`.
- [x] Run `ecs::Schedule` through `core::JobSystem`.
- [x] Keep `tf::Taskflow` inside `ecs::Schedule` as an implementation detail for dependency graph execution.
- [x] Ensure unit tests initialize `core::JobSystem`.

## Phase 3: RenderGraph Integration

- [x] Keep RenderGraph execution order deterministic through `m_ExecuteLayers`.
- [x] Use `core::JobSystem` only for CPU-side command recording inside a layer.
- [x] Keep queue submit, present, fence signaling, and transient resource retirement on the render thread.
- [x] Add profiling for per-layer record time and submit time.
- [x] Add tests with side-effect passes to prove layer ordering remains stable.

## Phase 4: Physics Integration

- [x] Add a Jolt `JobSystem` adapter backed by `core::JobSystem`.
- [x] Keep `PhysicsWorld::Step` fixed-tick controlled by game/simulation flow.
- [x] Avoid a standalone physics main-loop thread unless profiling shows a clear need.

## Phase 5: Asset and Coroutine Layer

- [x] Add async asset jobs on top of `core::JobSystem`.
- [x] Consider coroutine `Task<T>` only for IO/streaming flow, not ECS or RenderGraph parallel execution.
- [x] Add cancellation and lifetime ownership before exposing coroutine APIs.

## Cleanup

- [x] Decide whether Taskflow remains the permanent backend or is hidden behind a custom scheduler implementation.
- [x] Document thread ownership rules for ECS world writes, render snapshots, GPU resource creation, and resource upload.
