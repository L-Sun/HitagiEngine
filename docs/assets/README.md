# hitagi::asset module

`hitagi::asset` is the engine's runtime resource layer. It does three things: turn cooked data into runtime objects, manage the GPU residency of those objects, and give the layers above a way to look them up and deduplicate them by identity.

What it does **not** do matters just as much: it does not parse USD, compile material graphs, or understand concrete shading models such as PBR or Toon. Those belong to `hitagi::editor` (import and cook) and to the game or renderer (material semantics).

## Documents

| Document | Contents |
| --- | --- |
| [animation.md](animation.md) | Joint entities, shared clips/skins, playback and CPU skinning, render snapshots and current limits |
| [resource_lifecycle.md](resource_lifecycle.md) | The `Resource` base class, the `ResourceLoadState` state machine, Load/Unload behavior per resource, asynchronous texture decode, and the `AssetManager` registry and unload policy |
| [material.md](material.md) | The `Material` / `MaterialPass` boundary, the parameter-value model, GPU ABI packing rules for `material_data`, and the placeholder-texture mechanism |
| [cooked_binary_format.md](cooked_binary_format.md) | The HTGC binary container specification, and the full contract between the writer (editor cook) and the reader (runtime parser) |

## Module structure

`asset` is a C++20 module with one interface and many partitions. `asset.cppm` only re-exports:

```text
asset.cppm
  :cooked_format   HTGC record POD definitions (the single source of truth for the format)
  :resource        Resource base class + ResourceLoadContext + ResourceLoadState
  :image_codec     ImageCodec interface + BMP/JPEG/PNG/TGA implementations
  :texture         Texture
  :material        Material / MaterialPass / MaterialParameterValue
  :mesh            VertexArray / IndexArray / Mesh
  :camera          CameraComponent
  :light           LightComponent
  :transform       Transform / RelationShip / MetaInfo (ECS components and systems)
  :animation       AnimationClip / AnimationPlayer / AnimationTarget / AnimationSystem
  :skin            Skin / SkinnedMesh / SkinningSystem
  :scene           Scene (owns an ecs::World)
  :shader          Shader
  :pipeline        RenderPipeline / ComputePipeline
  :manager         AssetManager
```

There is also an implementation partition, `:cooked_binary`, that is not exported. It implements `ParseCookedMaterial` / `ParseCookedScene`. It is invisible outside the module; `:manager` uses it through `import :cooked_binary;`.

## Resource types

`Resource::Type` covers every resource:

```text
Texture  Material  Vertex  Index  Mesh
Scene   Shader
RenderPipeline     ComputePipeline
AnimationClip      Skin
```

`AnimationClip` and `Skin` have no GPU resources. Their `Load` only sets the state to `Loaded`, so they still go through the same lifecycle interface.

## Dependency direction

```text
asset -> core   (Buffer, FileIOManager, JobSystem, RuntimeModule)
      -> gfx    (Device, Texture, GPUBuffer, Shader, RenderPipeline, BindlessHandle)
      -> math   (vec/mat/Color)
      -> ecs    (entity storage for Scene)
      -> utils  (UUID, EnumArray, optional_ref, Overloaded)
```

`asset` does not depend on `render`, `editor`, or `engine`. Those layers depend on it.

## Quick entry points

```cpp
// AssetManager is not a global singleton: FileIOManager and JobSystem are injected by the caller.
// Inside the engine: auto& assets = engine.Assets();
// Standalone (tools / tests):
core::FileIOManager file_io;
core::JobSystem     job_system;
asset::AssetManager assets(file_io, job_system, "assets");

auto scene    = assets.ImportScene("scenes/demo.hcscene");    // HTGC scene (validated by extension)
auto material = assets.ImportMaterial("materials/pbr.bin");   // HTGC material (validated by content)
auto texture  = assets.ImportTexture("textures/albedo.png");  // decode immediately
auto lazy     = assets.AcquireTexture("textures/albedo.png"); // lazy load + dedup

scene->Load({.device = device});   // establish GPU residency
assets.UnloadScene(scene);         // release GPU residency, keep CPU data
```

## Immutable geometry

Mesh, VertexArray and IndexArray accept complete geometry at construction. VertexArray::Data
is plain mutable construction storage, not a Resource; callers fill its attribute buffers
before transferring ownership. Passing construction data by value copies it; moving it
transfers ownership, and callers must stop using any borrowed writable views of that storage.
The resource exposes only const CPU spans. Mesh's vertex/index references and submesh list
are fixed after construction; range and referenced-index validation happens there.
There are no Modify, Resize, AddSubMesh or mutable Span APIs on these resources.

Load/Unload change GPU residency, never geometry content. Changes to instance bindings
belong in ECS components. USD import stages complete geometry and inherited skin influences
before publishing/deduplicating assets. CPU skinning builds a new vertex snapshot from
temporary buffers and publishes it only after completion; previous frames retain theirs.

## Scene composition

Scene owns a World and exposes Load, Unload, and GetWorld (plus construction and
inherited resource metadata). A newly constructed Scene has no entities. There
is no implicit root, current camera, or mesh/camera/light registry. Scene still
registers the built-in hierarchy, animation, transform, and
skinning systems; the host advances them with GetWorld().Update(jobs, seconds).

Create entities by composing components through EntityManager::Create(values...),
or Create() followed by Emplace. GetEntities<Component...>() returns a filtered
snapshot of current entities; it does not maintain a separate scene registry.
Transform(matrix) decomposes a matrix using the same TRS convention as import.
Names and parent relationships are ordinary MetaInfo and RelationShip components.
Parent edits are reflected in cached children during the next hierarchy update.
Destroying an entity through EntityManager destroys that entity only, not its
subtree; applications that need subtree deletion must explicitly select and
destroy those entities.

The USD Viewer indexes every top-level entity and combines their bounds. It owns
its viewing camera independently. HTGC v8 supports empty scenes and forests of
nodes, with no designated root or current camera. Re-cook older HTGC files.

## Definitions and runtime state

Resources retain immutable identity and construction-time definitions. Residency,
Material parameter values and Scene's owned World remain mutable runtime state.
Texture's source path is supplied at construction and cannot be reassigned.
CameraComponent stores projection; LightComponent stores type, color, intensity
and cone angles. Both use their entity Transform for position/orientation (local
forward -Z, up +Y), with no separate Camera/Light resource.

Editor CompilePipelineLayout reflects vertex inputs for DXIL and SPIR-V without
creating a device. It preserves explicit buffer layouts; automatic reflection
uses separate attribute buffers and rejects target-divergent layouts. Cook stores
the layout and all pipeline states. Runtime builds GPU objects without editing
the stored descriptor. Compiled shader bytecode is still produced at runtime.

SceneCookState::Current captures supported component state, material values and
entity overrides into a new cooked asset. It does not modify the original input
artifact, and is not a generic serializer for arbitrary ECS components.

CharacterPhysics and CharacterPhysicsSystem belong to physics:character. The
physics module uses asset scene components; asset does not import physics.
USD import registers this system when it creates character physics components.
Manual construction must register physics::CharacterPhysicsSystem on the World.
Bounds are Viewer-local derived data, computed from immutable current geometry
snapshots and never stored in engine asset components or cooked records.
