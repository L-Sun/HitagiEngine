# Thread Ownership Rules

## CPU Job Boundary

`core::JobSystem` is the only engine-owned CPU job boundary. Engine modules may submit jobs through it, but should not own independent thread pools for normal CPU work.

Taskflow is an implementation detail of `core::JobSystem` for now. Modules can keep local graph descriptions when useful, but execution must enter through `core::JobSystem`.

## ECS

`ecs::World::Update` runs schedules through `core::JobSystem`.

Systems may run in parallel only when their declared component access allows it. World structure changes, entity lifetime changes, and component writes must go through ECS APIs and schedule dependency declarations.

## Render

RenderGraph compilation and queue submission happen on the render thread.

Pass command recording may run on `core::JobSystem` within one `m_ExecuteLayers` layer. Resource state transitions are prepared serially before parallel recording so CPU-side resource state remains deterministic. Queue submit, present, fence signaling, and transient resource retirement stay on the render thread.

GPU resource creation and upload ownership stays with `gfx::Device` and render/asset integration points. Do not create GPU resources from arbitrary gameplay jobs.

## Physics

Jolt jobs run through a `core::JobSystem` adapter owned by `PhysicsWorld`.

`PhysicsWorld::Step` is explicitly driven by the game or simulation flow. There is no standalone physics main-loop thread; fixed timestep policy belongs to the caller.

## Asset Loading

Synchronous asset imports run on the calling thread.

Asynchronous asset imports use `core::JobSystem` and return future-based jobs with cancellation tokens. `AssetManager` owns the lifetime of submitted asset jobs and waits for them during destruction. Coroutine APIs should not be exposed until cancellation, ownership, and IO lifetime rules are extended beyond this future-based layer.
