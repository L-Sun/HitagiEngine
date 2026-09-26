# Resource lifecycle

A good runtime resource layer has to answer three questions: where this resource is right now (CPU or GPU), who decides when it goes up to the GPU, and who decides when it comes back down. `hitagi::asset` splits those three jobs across `ResourceLoadState`, `Resource::Load`, and `AssetManager` together with `Scene::Unload`.

The core rule is one sentence: **CPU data stays resident, GPU data is on demand**. `Unload` releases only the GPU side. Vertices, pixels, and parameters stay in memory, so a later `Load` is cheap.

## Resource base class

```cpp
struct ResourceLoadContext {
    gfx::Device& device;
};

class Resource : public std::enable_shared_from_this<Resource> {
public:
    enum struct Type : std::uint8_t { /* Texture, Material, ..., ComputePipeline */ };

    virtual void Load(const ResourceLoadContext& context) = 0;
    virtual void Unload()                                 = 0;

    auto GetType()  const noexcept -> Type;
    auto GetName()  const noexcept -> std::string_view;
    auto GetUUID()  const noexcept -> const utils::UUID&;

    auto GetLoadState() const noexcept -> ResourceLoadState;  // acquire
    auto IsSettled()    const noexcept -> bool;               // Loaded or Failed

protected:
    void SetLoadState(ResourceLoadState state) noexcept;      // release

private:
    std::atomic<ResourceLoadState> m_State{ResourceLoadState::Unloaded};
};
```

A few choices are deliberate:

* **`enable_shared_from_this` sits at the top of the hierarchy.** It can appear only once on an inheritance chain. A second occurrence makes the base ambiguous and makes `shared_from_this()` fail silently. Putting it on `Resource` lets any resource hand an asynchronous task a handle that owns it.
* **The state is a `std::atomic`, not a `bool`.** Texture decode writes the state on a worker thread and the render thread reads it, so it has to be atomic. `SetLoadState` uses release and `GetLoadState` uses acquire. That pair also publishes the decoded pixel data.
* **Copy is deleted, and move is written by hand.** `std::atomic` is not movable, so the move constructor and assignment move the state themselves (relaxed is enough, because only a resource that is not yet shared gets moved).
* **There is no wrapper such as `IsLoaded()`.** Call sites write `GetLoadState() == ResourceLoadState::Loaded` directly, so the state meaning stays visible at the call.

## ResourceLoadState

```text
Unloaded ──Load()──> Loading ──worker done──> Staged ──Load()──> Loaded
    │                    │                                          │
    │                    └──decode failed──> Failed                 │
    └──────────────synchronous resources arrive directly────────────┘

Unload() : any state ──> Unloaded
```

| State | Meaning |
| --- | --- |
| `Unloaded` | No GPU data; CPU source data may not be ready either |
| `Loading` | A background task is in flight (today, only texture decode enters this state) |
| `Staged` | CPU data is ready and waiting to be uploaded on the render thread |
| `Loaded` | The GPU resource is usable |
| `Failed` | Permanently failed; `Load()` does not retry |

`Loading` and `Staged` exist only on the asynchronous path. A synchronous resource goes from `Unloaded` straight to `Loaded` inside `Load()`.

The state describes **how resident** the resource is, not **whether the content is stale**. Derived classes track staleness themselves: `VertexArray` and `IndexArray` use a `dirty` flag, and `Material` uses `m_HasPendingTextures`.

## Load / Unload per resource

| Resource | `Load` | `Unload` |
| --- | --- | --- |
| `Texture` | Decode (optionally async) + `CreateTexture` + `CreateTextureView` | Release the view and texture, keep `m_ImageData` |
| `VertexArray` | Build a GPU buffer for each non-empty dirty attribute | Clear every `gpu_buffer` |
| `IndexArray` | Build the index GPU buffer; throw if the CPU buffer is empty | Clear `gpu_buffer` |
| `Mesh` | Cascade into `vertices` / `indices` | Cascade `Unload` into both |
| `Material` | Load texture parameters, then pipelines, then pack `material_data` | Clear `material_data` and cascade `pipeline->Unload()` |
| `Shader` | `device.CreateShader(desc)` | Release `gfx::Shader` |
| `RenderPipeline` | Load every shader, reflect the layout from the VS when it is missing, then `CreateRenderPipeline` | Release `gfx::RenderPipeline` |
| `ComputePipeline` | Load the shader, then `CreateComputePipeline` | Release the pipeline |
| `Scene` | Cascade `Mesh::Load` for every mesh entity | Cascade into each mesh **and** the material of each submesh |
| `Camera` / `Light` | No GPU resource; set `Loaded` directly | Set `Unloaded` directly |

`Scene::Load` returns early when the state is `Loaded`. `Scene::Unload` **deliberately has no state guard**: the renderer can load meshes and materials without going through `Scene::Load`, so the scene state can be `Unloaded` while GPU data really exists. Unload has to walk down unconditionally.

`Scene::Unload` cascades into materials, but **does not touch textures**. Textures are often shared by several scenes, so they are released by reference counting: the last owner reclaims them when it is destroyed.

## Asynchronous texture decode

Texture is the only resource that goes asynchronous, because it is the only one whose CPU work (file I/O plus image decode) is heavy enough.

The work is split in two:

```cpp
void Texture::DecodeCPU() noexcept;      // worker thread: call the injected ImageLoader, never touch gfx
void Texture::Upload(gfx::Device&);      // render thread: CreateTexture + CreateTextureView
```

`Load` drives the state machine and picks which half to run from the current state:

```text
Loaded / Failed         -> return immediately
Loading                 -> return immediately (in flight; the caller keeps using the placeholder texture)
Staged                  -> Upload(device) -> Loaded
Unloaded + ImageLoader  -> if a JobSubmitter was injected and the texture is held by shared_ptr:
                            SetLoadState(Loading); submitter(DecodeCPU)
                          otherwise DecodeCPU() synchronously, and throw runtime_error on failure
Unloaded + in-memory pixels -> Upload(device) directly
```

The asynchronous path requires the texture to be held by a `shared_ptr`: the task captures `std::static_pointer_cast<Texture>(weak_from_this().lock())`, and that owning handle keeps the object alive for the decode. A texture constructed on the stack cannot get that handle, so it falls back to the synchronous path.

Thread boundary:

```text
worker thread  : file I/O + decode, writes only m_ImageData, state Loading -> Staged/Failed
render thread  : every gfx call (CreateTexture/CreateTextureView), state Staged -> Loaded
```

Publishing `m_ImageData` across threads relies on the release/acquire pair of the state atomic. There is no extra lock.

A texture does not know about the file system or the thread pool. A lazy texture receives two **capabilities** at construction:

```cpp
using ImageLoader = std::function<ImageData()>;   // how to obtain pixels
Texture(path, ImageLoader loader, name = {}, core::JobSubmitter decode_submitter = {});
```

`AssetManager::AcquireTexture` passes `MakeFileImageLoader(m_FileIO, path)` (read through `FileIOManager` and pick a codec by extension) and `m_JobSystem.MakeSubmitter()`. `path` is only an identity (the dedup key, and the reference written at cook time). Whether and how the file is read is the loader's decision. `ResourceLoadContext` carries only `device`, not an executor. The caller of `Load` does not need to know the async policy; the code that created the texture decides that. A texture with no submitter takes the synchronous decode path. A texture with no loader has only in-memory pixels.

`Unload` is safe while a decode is in flight: the worker still finishes and pushes the state to `Staged`. The GPU side has already been released, the CPU data is kept, and the next `Load` uploads straight from `Staged`.

## AssetManager

`AssetManager` is a `core::RuntimeModule`. Its job is **identity** and **policy**, not ownership. It does not own `FileIOManager` or `JobSystem`. It holds references injected at construction, and it uses them to equip the textures it creates with capabilities (`ImageLoader` + `JobSubmitter`). `Scene` needs no service of its own: the executor is passed in at the call, through `Scene::Update(job_system)`.

```cpp
class AssetManager final : public core::RuntimeModule {
    AssetManager(core::FileIOManager& file_io, core::JobSystem& job_system, std::filesystem::path asset_root = {});

    // Import
    auto ImportScene(path)     -> std::shared_ptr<Scene>;      // .hcscene / .hitagiscene
    auto ImportTexture(path)   -> std::shared_ptr<Texture>;    // decode immediately
    auto ImportMaterial(path)  -> std::shared_ptr<Material>;   // HTGC material
    auto ImportTextureAsync(path, token) -> AssetLoadJob<std::shared_ptr<Texture>>;

    // Feed cooked bytes directly
    auto LoadCookedMaterial(std::span<const std::byte>, root = {}) -> std::shared_ptr<Material>;
    auto LoadCookedScene(std::span<const std::byte>, root = {})    -> std::shared_ptr<Scene>;

    // Identity and dedup
    auto AcquireTexture(path, name = {}) -> std::shared_ptr<Texture>;
    auto GetMaterial(std::string_view)   -> std::shared_ptr<Material>;
    auto FindResource(const utils::UUID&) -> std::shared_ptr<Resource>;

    // Unload
    void UnloadScene(const std::shared_ptr<Scene>&);
};
```

### The registry holds only weak references

```text
textures_by_path   : path (lexically_normal) -> weak_ptr<Texture>
materials_by_name  : name                    -> weak_ptr<Material>
resources_by_uuid  : UUID                    -> weak_ptr<Resource>
```

All three tables store `weak_ptr`. Registration **does not extend any resource's lifetime**. The real ownership chain is:

```text
Scene -> Mesh -> SubMesh.material -> Material -> MaterialParameter -> Texture
```

When the layer above drops `shared_ptr<Scene>`, the whole chain is reclaimed. Expired registry entries are cleared by `erase_if(expired)` on the next insertion.

### AcquireTexture is the dedup entry point

The same texture can be referenced by several materials in cooked data. Parsing does not `make_shared<Texture>` directly. It goes through the injected `CookedTextureResolver` callback, which calls `AcquireTexture`:

```text
hit and not expired -> return the same Texture instance
miss                 -> create a lazy Texture that only records the path, register it, and return it
```

So one path means one `Texture` object and one copy of GPU memory. A texture returned by `AcquireTexture` is **not decoded yet**. The file is read on the first `Load`. `ImportTexture` decodes immediately.

### Asynchronous import and cancellation

`ImportTextureAsync` is the manager's own async entry point, distinct from the decode task inside `Texture::Load`. It submits to `JobSystem` and returns an `AssetLoadJob`:

```cpp
struct AssetLoadJob<T> {
    std::shared_future<T> future;
    AssetLoadToken        token;   // RequestCancel / IsCancellationRequested
};
```

Cancellation is cooperative: the task checks the token once at the start and once before producing a result, and returns `nullptr` if cancelled. `TrackAsyncJob` records the future, and `WaitForAsyncJobs` waits for all of them in the destructor. That wait covers only `ImportTextureAsync`. The quiescence point for texture decode tasks is `JobSystem`'s own destructor.

### Unload policy

```cpp
void AssetManager::UnloadScene(const std::shared_ptr<Scene>& scene) {
    scene->Unload();                                   // cascade mesh + material + pipeline
    m_Registry.resources_by_uuid.erase(scene->GetUUID());
}
```

The caller also has to make the renderer drop its own cache (`IRenderer::InvalidateResources`). Otherwise the renderer may still hold GPU handles that have already been released.

Destruction order has one hard constraint: `AssetManager`'s destructor calls `Texture::DestroyDefaultTexture()`, and the placeholder texture holds GPU resources, so **`gfx::Device` must outlive `AssetManager`**. Declaring them in the wrong order in a test hits an access violation immediately.
