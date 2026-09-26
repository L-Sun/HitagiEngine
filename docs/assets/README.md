# hitagi::asset 模块文档

`hitagi::asset` 是引擎运行时的资源层。它只做三件事：把 cooked 数据变成运行时对象、
管理这些对象的 GPU 常驻状态、给上层提供按身份查找与去重的入口。

它**不做**的事同样重要：不解析 USD、不编译材质图、不理解 PBR/Toon 这类具体渲染模型。
这些属于 `hitagi::editor`（导入与 cook）和 game/renderer preset（材质语义）。

## 文档

| 文档 | 内容 |
| --- | --- |
| [resource_lifecycle.md](resource_lifecycle.md) | `Resource` 基类、`ResourceLoadState` 状态机、各资源的 Load/Unload 行为、纹理异步解码、`AssetManager` 注册表与卸载策略 |
| [material.md](material.md) | `Material` / `MaterialPass` 的边界、参数值模型、`material_data` 的 GPU ABI 打包规则、占位纹理机制 |
| [cooked_binary_format.md](cooked_binary_format.md) | HTGC 二进制容器格式规范，写入端（editor cook）与读取端（runtime parser）的完整约定 |

## 模块结构

`asset` 是单接口多分区的 C++20 模块。`asset.cppm` 只做 re-export：

```text
asset.cppm
  :cooked_format   HTGC 记录 POD 定义 (格式的唯一真相来源)
  :resource        Resource 基类 + ResourceLoadContext + ResourceLoadState
  :image_codec     ImageCodec 接口 + BMP/JPEG/PNG/TGA 实现
  :texture         Texture
  :material        Material / MaterialPass / MaterialParameterValue
  :mesh            VertexArray / IndexArray / Mesh / MeshFactory
  :camera          Camera
  :light           Light
  :transform       Transform / RelationShip / MetaInfo (ECS 组件与系统)
  :scene           Scene (持有 ecs::World)
  :shader          Shader
  :pipeline        RenderPipeline / ComputePipeline
  :manager         AssetManager
```

另有一个不导出的实现分区 `:cooked_binary`，实现 `ParseCookedMaterial` /
`ParseCookedScene`。它对模块外不可见，`:manager` 通过 `import :cooked_binary;` 使用。

## 资源类型

`Resource::Type` 覆盖全部资源：

```text
Texture  Material  Vertex  Index  Mesh
Camera   Light     Scene   Shader
RenderPipeline     ComputePipeline
```

其中 `Camera` / `Light` 没有 GPU 资源，`Load` 只是把状态置为 `Loaded`，
以便统一走同一套生命周期接口。

## 依赖方向

```text
asset -> core   (Buffer, FileIOManager, JobSystem, RuntimeModule)
      -> gfx    (Device, Texture, GPUBuffer, Shader, RenderPipeline, BindlessHandle)
      -> math   (vec/mat/Color/AABB)
      -> ecs    (Scene 的实体存储)
      -> utils  (UUID, EnumArray, optional_ref, Overloaded)
```

`asset` 不依赖 `render`、`editor`、`engine`。反向依赖由上层建立。

## 入口速查

```cpp
// AssetManager 不是全局单例: 依赖 (FileIOManager / JobSystem) 由构造方显式注入。
// 引擎内: auto& assets = engine.Assets();
// 独立使用 (工具 / 测试):
core::FileIOManager file_io;
core::JobSystem     job_system;
asset::AssetManager assets(file_io, job_system, "assets");

auto scene    = assets.ImportScene("scenes/demo.hcscene");    // HTGC 场景 (按扩展名校验)
auto material = assets.ImportMaterial("materials/pbr.bin");   // HTGC 材质 (按内容校验)
auto texture  = assets.ImportTexture("textures/albedo.png");  // 立即解码
auto lazy     = assets.AcquireTexture("textures/albedo.png"); // 懒加载 + 去重

scene->Load({.device = device});   // 建立 GPU 常驻
assets.UnloadScene(scene);         // 释放 GPU 常驻, 保留 CPU 数据
```
