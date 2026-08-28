# 材质

`hitagi::asset::Material` 是引擎运行时材质资源。它**不描述** PBR、Toon、Unlit 这类
渲染模型，也不保存 shade graph。它只有两样东西：一张命名参数值表，
以及若干条「面向某个 pass contract 的 GPU 绑定约定」。

具体材质语义属于更外层：

```text
game / renderer preset
  拥有材质语义、图降级策略、pass contract 的定义

editor / importer / cooker
  读取 authoring 数据 (USD 等), 调用 preset processor, 产出 cooked 材质

engine asset::Material
  存参数值, 存 pass 绑定约定, 按绑定顺序打包 material_data
```

所以 `hitagi::asset` 里不会出现 `MaterialModel`、`PBRMaterialDesc`、
`ToonMaterialDesc` 这类声明。

## 数据模型

```cpp
using MaterialParameterValue = std::variant<
    float, std::int32_t, std::uint32_t,
    math::vec2i, math::vec2u, math::vec2f,
    math::vec3i, math::vec3u, math::vec3f,
    math::vec4i, math::vec4u, math::vec4f,
    math::Color, math::mat4f,
    std::shared_ptr<Texture>>;

struct MaterialParameter {
    std::pmr::string       name;
    MaterialParameterValue value;
};

struct MaterialPass {
    std::pmr::string                   pass_contract;
    std::shared_ptr<RenderPipeline>    pipeline;
    std::pmr::vector<std::pmr::string> bindings;
    core::Buffer                       material_data;
};

class Material : public Resource {
public:
    Material(MaterialParameters parameters = {},
             std::pmr::vector<MaterialPass> passes = {},
             std::string_view name = "");

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    auto GetParameters() const noexcept -> std::span<const MaterialParameter>;
    auto GetPasses()     const noexcept -> std::span<const MaterialPass>;
    auto FindPass(std::string_view pass_contract) const noexcept -> const MaterialPass*;

    template <MaterialParametric T> void SetParameter(std::string_view name, T value) noexcept;
    template <MaterialParametric T> auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;
};
```

构造函数会按 name 去重参数（保留首次出现），并深拷贝 passes。

## bindings 才是 GPU ABI

`MaterialParameters` 只是值表，它的存储顺序**不是** shader ABI。真正的顺序由
每个 pass 的 `bindings` 决定：

```text
Material.parameters              (值表, 顺序无意义)
  base_color = Color(...)
  roughness  = 0.4
  albedo     = Texture(...)
  unused     = 1.0

MaterialPass("DemoPBRForward").bindings   (ABI, 顺序即布局)
  base_color
  roughness
  albedo
        ↓
  material_data : [base_color(16B)][roughness(4B)][albedo handle(4B)] + 对齐
```

没有出现在 `bindings` 里的参数（例子中的 `unused`）不进入这个 pass 的
`material_data`；`bindings` 里找不到对应参数的名字会被跳过。

同一个 material 可以有多条 pass，各自有独立的 `bindings` 和 `material_data`。
renderer 用 `FindPass("自己约定的 contract")` 取，取不到就跳过这个材质——
不会从参数名反推渲染模型，也不会临时合成 pass。

`MaterialPass` 不是 RenderGraph 的 pass。它是「某个材质面向某个 pass contract 的
绑定 ABI」，是数据，不是执行节点。

## material_data 的打包规则

`GenerateMaterialData` 按 `bindings` 顺序逐个写入：

* 标量/向量/矩阵：按 `sizeof(T)` 原样 `memcpy`。
* 纹理：写入 `gfx::BindlessHandle`，占 `sizeof(gfx::BindlessHandle)` 字节。
* DX12 后端启用 16 字节打包：若当前 offset 到下一个 16 字节边界的剩余空间
  装不下这个值，先补齐到边界再写，避免值跨越边界。Vulkan 后端不做这个调整。
* 缓冲总大小为 `max(16, align(offset, 16))`，`bindings` 为空则不分配。
* 缓冲在写入前整体清零，未命中的绑定留 0。

后端差异由 `context.device.device_type == gfx::Device::Type::DX12` 决定，
所以同一个材质在不同后端上打出的字节布局可以不同——这正是 `material_data`
必须在运行时生成、而不能在 cook 时固化的原因。

## 占位纹理与重打包

纹理可能还在后台解码，而这一帧就要把 bindless handle 写进 `material_data`。
处理方式是占位 + 重打包，不需要纹理反向通知材质：

```text
Frame 1  Material::Load
           纹理 Load -> Loading (在飞)
           pending = true
           DefaultTexture 确保常驻, 打包占位 handle
           m_HasPendingTextures = true, 状态置 Loaded

Frame 2  Material::Load
           因 m_HasPendingTextures 为真, 不短路
           纹理 Load -> Staged -> Upload -> Loaded
           pending = false, 重新打包真实 handle

Frame 3  Material::Load
           Loaded 且无 pending -> 直接返回, handle 保持稳定
```

判定用的是 `Texture::IsSettled()`（`Loaded` 或 `Failed`），所以解码失败的纹理
不会让材质永远重打包下去——它会稳定在占位 handle 上。

`GenerateMaterialData` 里对纹理还有一层兜底：拿不到 GPU view 且纹理非空时，
直接取 `Texture::DefaultTexture()` 的 handle，保证 shader 采样到的永远是有效资源。

## Load / Unload

```cpp
void Material::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && !m_HasPendingTextures) return;

    bool pending = false;
    for (auto& parameter : m_Parameters) {
        // 非空纹理参数逐个 Load, 未 settle 则标记 pending
    }
    if (pending) Texture::DefaultTexture()->Load(context);

    for (auto& pass : m_Passes) {
        if (pass.pipeline) pass.pipeline->Load(context);
        pass.material_data = GenerateMaterialData(pass, /* DX12 打包 */);
    }
    m_HasPendingTextures = pending;
    SetLoadState(ResourceLoadState::Loaded);
}
```

`Unload` 清空所有 `material_data` 并级联 `pipeline->Unload()`，但不碰纹理——
纹理常被多材质共享，由引用计数回收。

`SetParameter` 会调用 `InvalidatePassData()`：清空全部 `material_data` 并把状态
退回 `Unloaded`，下一次 `Load` 自然重建。

## pipeline 挂在 pass 上

shader 描述不直接挂在 `MaterialPass`，而是由 `asset::RenderPipeline` 持有
（`gfx::RenderPipelineDesc` + `asset::Shader` 列表）。`RenderPipeline::Load` 负责：

```text
1. 逐个 Load 持有的 Shader (device.CreateShader)
2. desc.vertex_input_layout 为空时, 从 vertex shader 反射出 layout
3. device.CreateRenderPipeline(desc, shaders)
```

renderer 取 pipeline 的顺序是：优先 `MaterialPass.pipeline->GetBuiltPipeline()`，
没有时回退到 renderer 自带的默认 pipeline。
