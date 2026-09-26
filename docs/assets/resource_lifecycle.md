# 资源生命周期

一个好的运行时资源层，要能回答三个问题：这份资源现在在哪（CPU 还是 GPU）、
谁在决定它何时上 GPU、谁在决定它何时下来。`hitagi::asset` 把这三件事分别交给
`ResourceLoadState`、`Resource::Load`、`AssetManager` 与 `Scene::Unload`。

核心约定只有一句：**CPU 数据常驻，GPU 数据按需**。`Unload` 只释放 GPU 侧，
CPU 侧的顶点、像素、参数留在内存里，所以重新 `Load` 很便宜。

## Resource 基类

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
    auto IsSettled()    const noexcept -> bool;               // Loaded 或 Failed

protected:
    void SetLoadState(ResourceLoadState state) noexcept;      // release

private:
    std::atomic<ResourceLoadState> m_State{ResourceLoadState::Unloaded};
};
```

几个刻意的设计选择：

* **`enable_shared_from_this` 放在继承链顶端。** 它在一条继承链上只能出现一次，
  出现两次会让基类二义、`shared_from_this()` 静默失效。放在 `Resource` 上，
  任何资源都能向异步任务交出一个持有自己所有权的句柄。
* **状态是 `std::atomic`，不是 `bool`。** 纹理解码在 worker 线程写状态、
  渲染线程读状态，必须原子。`SetLoadState` 用 release、`GetLoadState` 用 acquire，
  这对内存序同时负责发布解码出来的像素数据。
* **拷贝删除、移动手写。** `std::atomic` 不可移动，所以移动构造/赋值手工搬运状态
  （relaxed 即可，只有尚未共享的资源才会被移动）。
* **没有 `IsLoaded()` 之类的包装。** 调用点直接写
  `GetLoadState() == ResourceLoadState::Loaded`，状态语义留在调用点可见。

## ResourceLoadState

```text
Unloaded ──Load()──> Loading ──worker 完成──> Staged ──Load()──> Loaded
    │                    │                                          │
    │                    └──解码失败──> Failed                       │
    └──────────────同步资源直接到达────────────────────────────────┘

Unload() : 任意状态 ──> Unloaded
```

| 状态 | 含义 |
| --- | --- |
| `Unloaded` | 没有 GPU 数据；CPU 源数据也可能尚未就绪 |
| `Loading` | 后台任务在飞（目前只有纹理解码会进入） |
| `Staged` | CPU 数据就绪，等待在渲染线程上传 |
| `Loaded` | GPU 资源可用 |
| `Failed` | 永久失败，`Load()` 不再重试 |

`Loading` / `Staged` 只出现在异步路径上。同步资源在 `Load()` 内部从 `Unloaded`
直接到 `Loaded`。

状态描述的是**常驻程度**，不是**内容新旧**。内容是否过期由派生类自己管：
`VertexArray`/`IndexArray` 用 `dirty` 标记，`Material` 用 `m_HasPendingTextures`。

## 各资源的 Load / Unload

| 资源 | `Load` | `Unload` |
| --- | --- | --- |
| `Texture` | 解码（可异步）+ `CreateTexture` + `CreateTextureView` | 释放 view 与 texture，保留 `m_ImageData` |
| `VertexArray` | 对每个非空且 dirty 的属性建 GPU buffer | 清空全部 `gpu_buffer` |
| `IndexArray` | 建索引 GPU buffer；CPU 缓冲为空时抛异常 | 清空 `gpu_buffer` |
| `Mesh` | 级联 `vertices` / `indices` | 级联两者的 `Unload` |
| `Material` | 加载纹理参数 → 加载 pipeline → 打包 `material_data` | 清空 `material_data`，级联 `pipeline->Unload()` |
| `Shader` | `device.CreateShader(desc)` | 释放 `gfx::Shader` |
| `RenderPipeline` | 加载所有 shader → 缺 layout 时从 VS 反射 → `CreateRenderPipeline` | 释放 `gfx::RenderPipeline` |
| `ComputePipeline` | 加载 shader → `CreateComputePipeline` | 释放 pipeline |
| `Scene` | 级联所有 mesh 实体的 `Mesh::Load` | 级联 mesh **和** 其 submesh 的 material |
| `Camera` / `Light` | 无 GPU 资源，直接置 `Loaded` | 直接置 `Unloaded` |

`Scene::Load` 会在 `Loaded` 时短路，`Scene::Unload` 则**故意不加状态守卫**：
renderer 会绕过 `Scene::Load` 直接加载 mesh 和 material，所以场景状态可能是
`Unloaded` 而 GPU 数据确实存在。卸载必须无条件下潜。

`Scene::Unload` 级联到 material，但**不碰纹理**：纹理常被多个场景共享，
它的释放交给引用计数——最后一个持有者析构时自然回收。

## 纹理的异步解码

纹理是唯一走异步的资源，因为只有它的 CPU 侧工作（文件 IO + 图像解码）足够重。

拆分成两半：

```cpp
void Texture::DecodeCPU() noexcept;      // worker 线程: 调用注入的 ImageLoader, 绝不碰 gfx
void Texture::Upload(gfx::Device&);      // 渲染线程: CreateTexture + CreateTextureView
```

`Load` 是状态机的驱动，按当前状态决定做哪一半：

```text
Loaded / Failed        -> 直接返回
Loading                -> 直接返回 (在飞, 调用方继续用占位纹理)
Staged                 -> Upload(device) -> Loaded
Unloaded + ImageLoader  -> 注入了 JobSubmitter 且被 shared_ptr 持有:
                            SetLoadState(Loading); submitter(DecodeCPU)
                          否则同步 DecodeCPU(), 失败抛 runtime_error
Unloaded + 内存像素     -> 直接 Upload(device)
```

异步的前提是这张纹理被 `shared_ptr` 持有：任务捕获
`std::static_pointer_cast<Texture>(weak_from_this().lock())`，用所有权句柄保证
解码期间对象不会析构。栈上构造的纹理拿不到句柄，自动退回同步路径。

线程边界：

```text
worker 线程 : 文件 IO + 解码, 只写 m_ImageData, 状态 Loading -> Staged/Failed
渲染线程    : 一切 gfx 调用 (CreateTexture/CreateTextureView), 状态 Staged -> Loaded
```

`m_ImageData` 的跨线程发布依赖状态原子的 release/acquire 配对，没有额外的锁。

纹理不认识文件系统，也不认识线程池。懒加载纹理在构造时接收两个**能力**：

```cpp
using ImageLoader = std::function<ImageData()>;   // 怎么拿到像素
Texture(path, ImageLoader loader, name = {}, core::JobSubmitter decode_submitter = {});
```

`AssetManager::AcquireTexture` 传入 `MakeFileImageLoader(m_FileIO, path)`（经 `FileIOManager`
读文件 + 按扩展名选 codec）与 `m_JobSystem.MakeSubmitter()`。`path` 只是身份（去重键、cook 时写出的引用），
读不读、怎么读由 loader 决定。`ResourceLoadContext` 里只有 `device`，不携带执行器——`Load`
的调用方无需关心异步策略，异步能力由创建纹理的一方决定。没有 submitter 的纹理走同步解码路径；
没有 loader 的纹理只有内存像素。

`Unload` 在解码在飞时也是安全的：worker 仍会完成并把状态推到 `Staged`，
而 GPU 侧本来就已释放，CPU 数据保留，下次 `Load` 直接从 `Staged` 上传。

## AssetManager

`AssetManager` 是 `core::RuntimeModule`，职责是**身份**与**策略**，不是所有权。
它不持有 `FileIOManager` / `JobSystem`，只持有构造时注入的引用，并据此给自己创建的纹理
配好能力（`ImageLoader` + `JobSubmitter`）。`Scene` 不需要任何服务：执行器在
`Scene::Update(job_system)` 时按调用传入。

```cpp
class AssetManager final : public core::RuntimeModule {
    AssetManager(core::FileIOManager& file_io, core::JobSystem& job_system, std::filesystem::path asset_root = {});

    // 导入
    auto ImportScene(path)     -> std::shared_ptr<Scene>;      // .hcscene / .hitagiscene
    auto ImportTexture(path)   -> std::shared_ptr<Texture>;    // 立即解码
    auto ImportMaterial(path)  -> std::shared_ptr<Material>;   // HTGC 材质
    auto ImportTextureAsync(path, token) -> AssetLoadJob<std::shared_ptr<Texture>>;

    // 直接喂 cooked 字节
    auto LoadCookedMaterial(std::span<const std::byte>, root = {}) -> std::shared_ptr<Material>;
    auto LoadCookedScene(std::span<const std::byte>, root = {})    -> std::shared_ptr<Scene>;

    // 身份与去重
    auto AcquireTexture(path, name = {}) -> std::shared_ptr<Texture>;
    auto GetMaterial(std::string_view)   -> std::shared_ptr<Material>;
    auto FindResource(const utils::UUID&) -> std::shared_ptr<Resource>;

    // 卸载
    void UnloadScene(const std::shared_ptr<Scene>&);
};
```

### 注册表只持弱引用

```text
textures_by_path   : path (lexically_normal) -> weak_ptr<Texture>
materials_by_name  : name                    -> weak_ptr<Material>
resources_by_uuid  : UUID                    -> weak_ptr<Resource>
```

三张表全部是 `weak_ptr`，注册**不延长任何资源的生命周期**。真正的所有权链是：

```text
Scene -> Mesh -> SubMesh.material -> Material -> MaterialParameter -> Texture
```

上层放开 `shared_ptr<Scene>`，整条链自然回收，注册表里的条目在下次插入时被
`erase_if(expired)` 清掉。

### AcquireTexture 是去重入口

cooked 数据里同一张贴图会被多个材质引用。解析时不直接 `make_shared<Texture>`，
而是通过注入的 `CookedTextureResolver` 回调走 `AcquireTexture`：

```text
命中且未过期 -> 返回同一个 Texture 实例
未命中       -> 创建一个只记路径的懒加载 Texture, 登记后返回
```

于是「同一路径 = 同一个 `Texture` 对象 = 一份 GPU 内存」。注意 `AcquireTexture`
返回的纹理是**未解码**的，首次 `Load` 时才读文件；`ImportTexture` 则是立即解码。

### 异步导入与取消

`ImportTextureAsync` 是 manager 自己的异步入口（区别于 `Texture::Load` 内部的
解码任务）。它提交到 `JobSystem`，返回 `AssetLoadJob`：

```cpp
struct AssetLoadJob<T> {
    std::shared_future<T> future;
    AssetLoadToken        token;   // RequestCancel / IsCancellationRequested
};
```

取消是协作式的：任务在开始时和产出前各检查一次 token，被取消则返回 `nullptr`。
`TrackAsyncJob` 记录 future，`WaitForAsyncJobs` 在析构时等待全部完成——这只服务于
`ImportTextureAsync`，纹理解码任务的静止点由 `JobSystem` 自己的析构保证。

### 卸载策略

```cpp
void AssetManager::UnloadScene(const std::shared_ptr<Scene>& scene) {
    scene->Unload();                                   // 级联 mesh + material + pipeline
    m_Registry.resources_by_uuid.erase(scene->GetUUID());
}
```

调用方还需要让 renderer 丢弃自己的缓存（`IRenderer::InvalidateResources`），
否则 renderer 可能还握着已经释放的 GPU 句柄。

析构顺序有一条硬约束：`AssetManager` 的析构会调用
`Texture::DestroyDefaultTexture()`，而占位纹理持有 GPU 资源，**`gfx::Device`
必须比 `AssetManager` 活得久**。测试里声明顺序写反会直接触发访问违例。
