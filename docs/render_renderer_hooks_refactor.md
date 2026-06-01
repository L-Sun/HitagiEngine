# Renderer RenderResult + Hook Refactor Plan

## 背景

当前 `DefaultRenderer` 已经演进为 deferred renderer，并且内部混入了 editor selection metadata、selection outline、viewport grid 等 editor-only pass。这样会带来几个问题：

- `hitagi/render` 模块依赖 editor 语义，职责边界不清晰。
- editor 功能只能通过修改 renderer 内部实现接入，扩展成本高。
- `SceneView` 携带 `EditorSelectionDesc`，导致 runtime render API 暴露 editor 业务类型。
- `SceneDrawState`、frame constant、G-buffer 等中间资源只存在于 `DeferredRenderer` 内部，当前 selection metadata pass 因此被迫放进 renderer。

重构目标不是让所有 renderer 都实现同一套内部 hook。更稳的方向是：

1. `IRenderer` 统一返回 `RenderResult`，让 editor 优先通过 renderer 输出的通用资源做外部组合。
2. 只有必须进入 renderer 内部阶段的功能，才使用 renderer-specific hook。
3. `DeferredRenderer` 可以提供 deferred-only hook，但通用 `IRenderer` 协议不暴露 G-buffer 概念。

## 设计目标

1. `hitagi/render` 不包含 editor 业务类型和 editor shader。
2. `IRenderer` 面向 engine editor 和 game renderer 提供统一 viewport render 协议。
3. Editor 默认消费 `RenderResult` 中的 color、depth、object id 等通用输出，在 renderer 外追加 grid、gizmo、outline、debug overlay。
4. Deferred-only 中间资源只通过 `DeferredRenderer` 专属扩展点暴露，避免污染 forward renderer、自定义 game renderer、2D renderer 等实现。
5. 默认 runtime、playground、tests 不依赖 editor 模块。

## 核心接口

### Render Request / Result

用 `RenderRequest` 和 `RenderResult` 替代当前 `Render(context, view, target) -> TextureHandle`：

```cpp
struct RenderOutputMask {
    bool depth         = false;
    bool linear_depth  = false;
    bool object_id     = false;
    bool normal        = false;
    bool motion_vector = false;
};

struct SceneView {
    std::shared_ptr<asset::Scene> scene;
    const asset::Camera*          camera = nullptr;
    math::mat4f                   camera_transform;
};

struct RenderRequest {
    SceneView        view;
    rg::TextureHandle target;
    RenderOutputMask requested_outputs;
};

struct RenderResult {
    rg::TextureHandle color;
    rg::TextureHandle depth;
    rg::TextureHandle linear_depth;
    rg::TextureHandle object_id;
    rg::TextureHandle normal;
    rg::TextureHandle motion_vector;
};

class IRenderer : public core::RuntimeModule {
public:
    using core::RuntimeModule::RuntimeModule;
    virtual ~IRenderer() = default;

    virtual auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult = 0;
};
```

约定：

- `color` 是最终 scene color。
- 其余 handle 是可选输出，没有生成时保持默认空 handle。
- `requested_outputs` 是请求，不是强制能力。renderer 可以按自身能力返回可用资源。
- `SceneView` 不再包含 `EditorSelectionDesc` 或任何 editor 状态。

### Deferred Renderer Hook

当前 editor selection metadata pass 需要复用 `DeferredRenderer` 内部的 `SceneDrawState`、frame constant 和 mesh draw data。这个能力不适合放进通用 `IRenderer`，因为不是所有 renderer 都有 G-buffer 或相同 draw state。

因此 hook 作为 `DeferredRenderer` 的可选扩展能力：

```cpp
struct DeferredRenderResources {
    rg::TextureHandle color;
    rg::TextureHandle depth;
    rg::TextureHandle linear_depth;
    rg::TextureHandle gbuffer_albedo;
    rg::TextureHandle gbuffer_normal;
    rg::TextureHandle gbuffer_material;
    rg::TextureHandle gbuffer_emissive;
    rg::GPUBufferHandle frame_constant;
    rg::SamplerHandle sampler;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct DeferredSceneDrawData {
    std::span<const InstanceInfo> instances;
    const SceneDrawState* scene_draw_state = nullptr; // long term: readonly view
};

class IDeferredRenderExtension {
public:
    virtual ~IDeferredRenderExtension() = default;

    virtual auto AfterGBuffer(
        RenderContext& context,
        const SceneView& view,
        const DeferredRenderResources& resources,
        const DeferredSceneDrawData& draw_data) -> void {}

    virtual auto AfterLighting(
        RenderContext& context,
        const SceneView& view,
        DeferredRenderResources& resources,
        const DeferredSceneDrawData& draw_data) -> void {}
};
```

`AfterGBuffer` 用于 selection metadata、custom selected-object masks、debug capture 等必须依赖 deferred draw data 的 pass。

`AfterLighting` 用于必须接在 deferred lighting 后、且需要改写 `resources.color` 的 pass。

这个接口属于 `DeferredRenderer`，不是所有 game renderer 的通用协议。自定义 game renderer 可以选择不支持它，也可以提供自己的 renderer-specific hook。

## Renderer 改造

1. `IRenderer::Render` 返回 `RenderResult`。

2. `DeferredRenderer::Render` 负责生成：

```cpp
RenderResult{
    .color = resources.color,
    .depth = resources.depth,
    .linear_depth = resources.linear_depth,
    .normal = resources.gbuffer_normal,
};
```

3. 如果后续实现 object picking buffer，优先作为 `RenderResult::object_id` 输出，而不是 editor selection buffer。

4. `DeferredRenderer` 持有 deferred-only extension list：

```cpp
void AddExtension(std::shared_ptr<IDeferredRenderExtension> extension);
void ClearExtensions();
```

5. `DeferredRenderer::Render` 在 G-buffer pass 完成后构造 `DeferredRenderResources`，调用 `AfterGBuffer`。

6. Deferred lighting 后把 `resources.color` 设置为 lighting 输出，再调用 `AfterLighting`。扩展如果生成新 color target，只更新 `resources.color`。

7. `SceneView` 移除 `editor_selection` 字段，只保留 scene、camera、camera transform 等 runtime view data。

## Editor 侧迁移

### 默认路径：RenderResult 外部组合

当前 editor 中多数功能不需要 renderer 内部 hook：

- viewport grid：只需要 scene color，最好再消费 depth。
- transform gizmo / selected axis：当前是 ImGui draw list overlay。
- picking：当前是 CPU raycast `PickEditorEntity`。
- inspector、asset browser、debug panel：不需要 renderer hook。

Editor viewport 默认这样组织：

```cpp
auto result = renderer.Render(context, RenderRequest{
    .view = scene_view,
    .target = scene_render_texture,
    .requested_outputs = {
        .depth = true,
        .object_id = true,
        .normal = true,
    },
});

auto color = result.color;
color = grid_pass.Build(context, camera, camera_transform, color, result.depth);
color = gizmo_pass.Build(context, color, result.depth);
```

如果 selection outline 改成基于 `object_id + depth` 的简单版本，也可以完全放在 renderer 外：

```cpp
color = selection_outline.Build(
    context,
    color,
    result.depth,
    result.object_id,
    selection_service.Selected());
```

### 高质量 selection outline：Deferred hook

如果要保留当前 selected mesh 重绘、selected depth、occluded outline/highlight 这套效果，则需要 `DeferredRenderer` 专属 hook。

在 `examples/editor` 或未来 `hitagi/editor` 中新增：

```cpp
class EditorDeferredSelectionExtension final : public render::IDeferredRenderExtension {
public:
    explicit EditorDeferredSelectionExtension(EditorSelectionService& selection);

    void AfterGBuffer(...) override;   // 生成 selection id / visual / depth buffers
    void AfterLighting(...) override;  // 根据 selection buffers 改写 resources.color
};
```

迁移内容：

- `EditorSelectionDesc`
- `EditorSelectionItem`
- `EditorSelectionVisual`
- `EditorSelectionMetadata`
- `SelectionOutline`
- `editor_selection_outline.hlsl`

这些都属于 editor feature，不属于 `hitagi/render`。

## Grid Pass 处理

Viewport grid 不需要访问 renderer 内部 scene draw state，应留在 editor viewport 中作为普通 post pass：

1. `SceneViewPort` 调 `renderer.Render(...)` 得到 `RenderResult`。
2. `EditorGrid::Build(...)` 在 editor 模块中追加 pass。
3. `EditorGrid` 类型和 `viewport_grid.hlsl` 从 `hitagi/render` 迁到 `examples/editor` 或未来 `hitagi/editor`。

## 分阶段落地

### Phase 1: RenderResult 协议

- 新增 `RenderRequest`、`RenderOutputMask`、`RenderResult`。
- 修改 `IRenderer::Render` 签名。
- `SceneView` 删除 `editor_selection`。
- `DeferredRenderer` 返回 color、depth、normal 等可用输出。
- 更新 playground、scene capture、renderer tests、editor viewport 调用点。

### Phase 2: 外部组合 editor pass

- 把 `EditorGrid` 从 `render::passes` 移到 editor。
- Editor viewport 基于 `RenderResult` 追加 grid/gizmo/overlay。
- 删除 render module 对 `viewport_grid.hlsl` 的认知。

### Phase 3: 迁移 selection 类型

- 把 `EditorSelectionDesc`、`EditorSelectionItem`、`EditorSelectionVisual` 移到 editor。
- 如果采用简单 object id outline，实现 `RenderResult::object_id` 并把 outline 完全放在 editor 外部组合。
- 如果保留当前高质量 outline，进入 Phase 4。

### Phase 4: Deferred-only selection hook

- 新增 `IDeferredRenderExtension`。
- `DeferredRenderer` 支持 extension list。
- 把 `EditorSelectionMetadata`、`SelectionOutline` 迁到 editor。
- editor 创建 `EditorDeferredSelectionExtension` 并注册到 `DeferredRenderer`。

### Phase 5: 文件拆分

建议拆分当前 `forward_renderer.cpp`：

- `render_runtime.cpp`
- `deferred_renderer.cpp`
- `passes/gbuffer.cpp`
- `passes/deferred_lighting.cpp`
- `passes/gbuffer_debug.cpp`
- `passes/present.cpp`
- `render_utils.cpp` 保持 GUI/text helper

## 风险点

- `RenderResult::object_id` 的语义需要稳定：它应表达 runtime object/entity identity，不表达 selected/hovered/editor visual 状态。
- `requested_outputs` 只是请求。Editor 必须处理 renderer 不提供某个输出时的降级路径。
- `SceneDrawState` 当前是 renderer 内部结构。deferred hook 如果直接暴露指针会降低封装性，长期应提供只读 view。
- Deferred hook 顺序必须稳定。建议 extension 按注册顺序执行，必要时后续增加 priority。
- Editor extension 需要使用 render graph transient resources，必须遵守只追加 pass、不提前 execute 的约束。
- 多 viewport 场景下 extension 状态不能全局共享，应按 viewport 或 render invocation 传入 selection state。
- 自定义 game renderer 不应该被迫实现 deferred hook；engine editor 应优先通过 `RenderResult` 整合。

## 推荐最终边界

- `hitagi/render`: `IRenderer`、`RenderRequest`、`RenderResult`、scene renderer、G-buffer、lighting、present、通用 debug view。
- `hitagi/render::DeferredRenderer`: 可选 `IDeferredRenderExtension`，只服务 deferred-specific 内部插入。
- `examples/editor` 或未来 `hitagi/editor`: selection state、selection outline、viewport grid、gizmo、editor camera overlay。
- `game_runtime`: 可实现自己的 `IRenderer`，只返回通用 `RenderResult`，不依赖 editor。
- `game_editor`: 把 game renderer 注册给 engine editor，并追加游戏专用工具/overlay。
- `hitagi/engine`: 只组装 runtime modules，不代理 render graph 细节。
