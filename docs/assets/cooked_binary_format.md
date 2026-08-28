# HTGC 二进制格式

HTGC（Hitagi Cooked）是引擎的 cooked 资产容器格式。它服务于一个很窄的目标：
**运行时加载不做解析**。顶点、索引、像素这类大块数据在文件里就是最终的内存布局，
加载时一次 `memcpy` 到位；结构性的元数据是定长 POD 记录，同样直接 `memcpy` 出来。

cooked 数据是**构建产物，不是归档格式**。版本不匹配时加载器直接报错要求重新 cook，
不做向后兼容。这换来的是格式可以随实现自由演进。

## 为什么不是 JSON

材质和场景曾经用 JSON。瓶颈不在结构性字段，而在顶点/索引数组：JSON 要为每个 float
做文本解析、每个元素做一次 push_back。同一份网格用二进制表达时，这部分退化成
一次 `memcpy`。这是格式重做的唯一动机。

## 文件布局

```text
┌────────────────────────────────────────────┐
│ CookedFileHeader          80 bytes         │  文件偏移 0
├────────────────────────────────────────────┤
│ meta section                               │  定长记录数组, 8 字节对齐
│   MaterialRecord / MeshRecord / PassRecord │
│   ParameterRecord / SubMeshRecord / ...    │
├────────────────────────────────────────────┤
│ string table                               │  裸字节, 无 NUL 结尾
├────────────────────────────────────────────┤
│ blob section                               │  16 字节对齐起始
│   顶点属性数据 / 索引数据                    │
└────────────────────────────────────────────┘
```

偏移约定分两层：

* header 里的三个 `BufferView` 存的是**绝对文件偏移**。
* 记录内部的 `ArrayRef` / `BufferView` / `StringRef` 存的是**所属 section 内的相对偏移**。

字节序：**只支持小端**，`cooked_format.cpp` 里有 `static_assert` 锁死。

## Header

```cpp
struct CookedFileHeader {           // 80 bytes
    std::array<char, 4> magic{};    // 'H','T','G','C'
    std::uint32_t       version;    // kCookedVersion, 当前为 1
    CookedAssetType     asset_type; // Material = 1, Scene = 2
    std::uint32_t       flags;
    BufferView          meta;       // 绝对文件偏移 + 长度
    BufferView          strings;
    BufferView          blob;
    std::uint64_t       root_offset;// 根记录在 meta section 内的偏移
    std::uint64_t       reserved;
};
```

`root_offset` 指向 `MaterialRecord`（材质文件）或 `SceneRecord`（场景文件），
是整个记录图的入口。

## 引用类型

```cpp
struct BufferView { std::uint64_t offset, size;  };  // 16B, 指向 blob
struct ArrayRef   { std::uint64_t offset, count; };  // 16B, 指向 meta 中的定长记录数组
struct StringRef  { std::uint32_t offset, length;};  //  8B, 指向 string table
```

`StringRef` 的 `length == 0` 表示空串。字符串表按内容去重，重复的名字只存一份。

## 记录

全部记录都是 trivially copyable POD，`CookedRecord` concept 强制这一点，
并且每个记录的 `sizeof` 都有 `static_assert` 锁死——加了字段忘了改版本号会直接编译失败。

### 材质侧

| 记录 | 大小 | 关键字段 |
| --- | --- | --- |
| `ParameterRecord` | 104 | `name`、`type`、`texture_name`/`texture_path`、`flags`、`payload[64]` |
| `ShaderRecord` | 40 | `name`、`type`、`entry`、`path`、`source` |
| `PipelineRecord` | 64 | `primitive`、`cull_mode`、`depth_compare`、`render_format`、`depth_stencil_format` + 三个布尔位 |
| `PassRecord` | 112 | `contract`、`has_pipeline`、`shaders`、`bindings`、内嵌 `PipelineRecord` |
| `MaterialRecord` | 40 | `name`、`parameters`、`passes` |

`ParameterRecord.payload` 是 64 字节的内联定长载荷。`MaterialParameterValue` 最大的
可选类型是 `mat4f`（64 字节），把它内联进定长记录，代价是少量浪费的字节，
收益是参数记录保持定长、可以整块 `AppendArray`。

纹理参数不存像素，只存 `texture_path`（相对于 asset root）与 `texture_name`；
空纹理用 `kCookedParameterNullTexture` 标志位表达，和「路径为空」区分开。

### 网格与场景侧

| 记录 | 大小 | 关键字段 |
| --- | --- | --- |
| `VertexAttributeRecord` | 24 | `attribute`（枚举名）、`data`（blob 视图） |
| `SubMeshRecord` | 32 | `index_count`、`index_offset`、`vertex_offset`、`material_index` |
| `MeshRecord` | 104 | `vertex_count`、`attributes`、`index_type`、`index_count`、`indices`、`sub_meshes`、`aabb_min/max` |
| `MeshInstanceRecord` | 80 | `name`、`mesh_index`、`transform` |
| `CameraRecord` | 124 | 投影参数 + `eye`/`look_dir`/`up` + `transform` |
| `LightRecord` | 144 | `type`、`intensity`、`color`、`position`/`direction`/`up`、锥角 |
| `SceneRecord` | 88 | `materials`、`meshes`、`instances`、`cameras`、`lights` |

`math::vec3f` / `math::Color` / `math::mat4f` 直接内嵌在记录里，不转成
`std::array<float, N>`——它们的 `sizeof`/`alignof` 同样被 `static_assert` 锁死，
读出来即可用，省掉一层 `bit_cast`。

`MeshRecord` 存了 AABB，加载时跳过 `ComputeAABB()`。这是 cook 期就能算完的东西。

`SubMeshRecord.material_index` 是 `SceneRecord.materials` 数组的下标，
`kCookedInvalidIndex`（`0xFFFFFFFF`）表示没有材质。

### 枚举一律存字符串

`gfx::ShaderType`、`gfx::CullMode`、`gfx::Format`、`asset::VertexAttribute`、
`asset::IndexType`、`Light::Type` 等在文件里都是字符串，读取时用
`magic_enum::enum_cast` 还原。

理由是失败模式的选择：存整数值时，C++ 侧重排枚举会让旧文件**静默解析成错误的值**；
存字符串时，同样的改动会让 `enum_cast` 失败，退回到明确的默认值或报错。
代价是几十字节的字符串表和一次查表，发生在加载期，可以忽略。

## 写入端

写入在 `hitagi/editor/cook.cppm`（`editor:cook` 分区），入口是：

```cpp
auto CookEditorMaterial(const EditorMaterial&, EditorCookOptions = {}) -> core::Buffer;
auto CookEditorScene(asset::Scene&, EditorCookOptions = {})            -> core::Buffer;
```

两个构建器：

* `StringTableBuilder`：`Add(string_view) -> StringRef`，按内容查表去重。
* `SectionBuilder`：`Append` / `AppendArray` 前自动对齐到 8 字节；
  `AppendBytes` 按调用方给的对齐（blob 用 16）。

装配顺序：

```text
meta_offset    = sizeof(CookedFileHeader)                       // 80
strings_offset = meta_offset + meta.size
blob_offset    = align_up(strings_offset + strings.size, 16)
```

注意记录数组是**先写子记录、再写父记录**的：`BuildMaterialRecord` 会先把
parameters/passes 数组 `AppendArray` 进 meta 拿到 `ArrayRef`，最后才把
`MaterialRecord` 本身 `Append` 进去，根记录的偏移作为 `root_offset` 写入 header。

路径统一由 `CookedPath` 转成相对于 `asset_root_path` 的 generic 形式；
落在 root 之外的路径保持原样。

## 读取端

读取在 `hitagi/asset/cooked_binary.cpp`（实现分区 `asset:cooked_binary`，
不对模块外导出）：

```cpp
auto ParseCookedMaterial(std::span<const std::byte>, root, CookedTextureResolver) -> std::shared_ptr<Material>;
auto ParseCookedScene(std::span<const std::byte>, root, CookedTextureResolver)    -> std::shared_ptr<Scene>;
```

`CookedReader` 在构造时完成全部头部校验：

```text
1. 长度 >= sizeof(CookedFileHeader)
2. magic == 'HTGC'
3. version == kCookedVersion            否则报错 "re-cook the asset"
4. asset_type == 期望类型
5. 三个 section 的绝对偏移与长度落在文件范围内
```

之后每次 `Read<T>` / `GetString` / `GetBlob` 都做一次范围检查，越界抛异常。
记录是 `memcpy` 出来的而不是原地强转指针——记录很小，拷贝可忽略，
换来的是完全不必操心文件里记录的对齐。

数组遍历用 lazy view，不额外分配：

```cpp
template <CookedRecord T>
auto ReadArray(ArrayRef ref) const {
    return std::views::iota(std::uint64_t{0}, ref.count) |
           std::views::transform([this, ref](std::uint64_t i) {
               return Read<T>(ref.offset + i * sizeof(T));
           });
}
```

### 纹理解析注入

解析器不自己 `make_shared<Texture>`，而是接受一个回调：

```cpp
using CookedTextureResolver =
    std::function<std::shared_ptr<Texture>(const std::filesystem::path&, std::string_view)>;
```

`AssetManager` 传入的实现是 `AcquireTexture`，于是同一路径的贴图在整个场景里
只有一个 `Texture` 实例。回调为空时（脱离 manager 解析，例如单元测试）
每次引用都建一个新的懒加载纹理。

`ResolveCookedPath` 负责路径还原：绝对路径原样返回；相对路径先看当前工作目录
是否存在，不存在则拼到 asset root 下。

## 演进规则

1. 任何记录布局的改动都必须 `+1` `kCookedVersion`。
2. 记录的 `static_assert(sizeof(...))` 要同步更新——它是防止「加字段忘改版本」的闸门。
3. 加字段优先放在记录尾部并显式补 padding，让对齐可预测。
4. 不要为了省字节把枚举改成整数存储（见上文的失败模式论证）。
5. cooked 文件不进版本库，改格式后重新 cook 即可。
