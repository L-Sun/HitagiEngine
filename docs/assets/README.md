# hitagi::asset module

`hitagi::asset` is the engine's runtime resource layer. It does three things: turn cooked data into runtime objects, manage the GPU residency of those objects, and give the layers above a way to look them up and deduplicate them by identity.

What it does **not** do matters just as much: it does not parse USD, compile material graphs, or understand concrete shading models such as PBR or Toon. Those belong to `hitagi::editor` (import and cook) and to the game or renderer preset (material semantics).

## Documents

| Document | Contents |
| --- | --- |
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
  :mesh            VertexArray / IndexArray / Mesh / MeshFactory
  :camera          Camera
  :light           Light
  :transform       Transform / RelationShip / MetaInfo (ECS components and systems)
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
Camera   Light     Scene   Shader
RenderPipeline     ComputePipeline
```

`Camera` and `Light` have no GPU resources. Their `Load` only sets the state to `Loaded`, so they still go through the same lifecycle interface.

## Dependency direction

```text
asset -> core   (Buffer, FileIOManager, JobSystem, RuntimeModule)
      -> gfx    (Device, Texture, GPUBuffer, Shader, RenderPipeline, BindlessHandle)
      -> math   (vec/mat/Color/AABB)
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
