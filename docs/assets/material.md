# Materials

`hitagi::asset::Material` is the engine's runtime material resource. It does **not** describe shading models such as PBR, Toon, or Unlit, and it does not store a shade graph. It holds two things: a table of named parameter values, and a set of GPU binding contracts, each aimed at one pass contract.

Concrete material semantics live further out:

```text
game / renderer preset
  owns material semantics, graph-lowering policy, and pass-contract definitions

editor / importer / cooker
  reads authoring data (USD and similar), runs the preset processor, and produces cooked materials

engine asset::Material
  stores parameter values, stores pass binding contracts, and packs material_data in binding order
```

So `hitagi::asset` does not declare types such as `MaterialModel`, `PBRMaterialDesc`, or `ToonMaterialDesc`.

## Data model

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

The constructor deduplicates parameters by name (keeping the first occurrence) and deep-copies the passes.

## bindings are the GPU ABI

`MaterialParameters` is only a value table. Its storage order is **not** the shader ABI. The real order comes from each pass's `bindings`:

```text
Material.parameters              (value table, order is meaningless)
  base_color = Color(...)
  roughness  = 0.4
  albedo     = Texture(...)
  unused     = 1.0

MaterialPass("DemoPBRForward").bindings   (ABI, order is the layout)
  base_color
  roughness
  albedo
        ↓
  material_data : [base_color(16B)][roughness(4B)][albedo handle(4B)] + alignment
```

A parameter that does not appear in `bindings` (such as `unused` above) is left out of that pass's `material_data`. A name in `bindings` that has no matching parameter is skipped.

One material can have several passes, each with its own `bindings` and `material_data`. The renderer fetches a pass with `FindPass("the contract it agreed on")`. If the pass is missing, that material is skipped. The renderer does not infer a shading model from parameter names, and it does not synthesize a pass on the spot.

`MaterialPass` is not a RenderGraph pass. It is the binding ABI of one material for one pass contract: data, not an execution node.

## material_data packing rules

`GenerateMaterialData` writes values in `bindings` order:

* Scalars, vectors, and matrices are `memcpy`'d as `sizeof(T)`.
* Textures are written as a `gfx::BindlessHandle`, occupying `sizeof(gfx::BindlessHandle)` bytes.
* The DX12 backend packs on 16-byte boundaries: if the space from the current offset to the next 16-byte boundary cannot hold the value, the writer pads to that boundary first so the value does not straddle it. The Vulkan backend does not do this.
* The buffer size is `max(16, align(offset, 16))`. An empty `bindings` list allocates nothing.
* The buffer is zeroed before writing, so unmatched bindings stay 0.

The backend difference is decided by `context.device.device_type == gfx::Device::Type::DX12`, so the same material can pack to a different byte layout on different backends. That is why `material_data` must be generated at runtime and cannot be baked at cook time.

## Placeholder textures and repacking

A texture may still be decoding in the background when this frame needs to write a bindless handle into `material_data`. The fix is a placeholder plus a later repack. The texture does not need to notify the material:

```text
Frame 1  Material::Load
           texture Load -> Loading (in flight)
           pending = true
           DefaultTexture is kept resident, and the placeholder handle is packed
           m_HasPendingTextures = true, state set to Loaded

Frame 2  Material::Load
           m_HasPendingTextures is true, so Load does not return early
           texture Load -> Staged -> Upload -> Loaded
           pending = false, repack the real handle

Frame 3  Material::Load
           Loaded and no pending -> return immediately, handle stays stable
```

The check uses `Texture::IsSettled()` (`Loaded` or `Failed`), so a texture that failed to decode does not make the material repack forever. It settles on the placeholder handle.

`GenerateMaterialData` has one more fallback for textures: if it cannot get a GPU view and the texture is non-null, it takes the handle of `Texture::DefaultTexture()`, so the shader always samples a valid resource.

## Load / Unload

```cpp
void Material::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && !m_HasPendingTextures) return;

    bool pending = false;
    for (auto& parameter : m_Parameters) {
        // Load each non-null texture parameter; mark pending if it has not settled
    }
    if (pending) Texture::DefaultTexture()->Load(context);

    for (auto& pass : m_Passes) {
        if (pass.pipeline) pass.pipeline->Load(context);
        pass.material_data = GenerateMaterialData(pass, /* DX12 packing */);
    }
    m_HasPendingTextures = pending;
    SetLoadState(ResourceLoadState::Loaded);
}
```

`Unload` clears every `material_data` and cascades `pipeline->Unload()`. It does not touch textures. Textures are often shared by several materials and are reclaimed by reference counting.

`SetParameter` calls `InvalidatePassData()`: it clears every `material_data` and moves the state back to `Unloaded`, so the next `Load` rebuilds it.

## The pipeline lives on the pass

Shader descriptions are not stored directly on `MaterialPass`. `asset::RenderPipeline` holds them (`gfx::RenderPipelineDesc` plus a list of `asset::Shader`). `RenderPipeline::Load` does this:

```text
1. Load each owned Shader (device.CreateShader)
2. If desc.vertex_input_layout is empty, reflect the layout from the vertex shader
3. device.CreateRenderPipeline(desc, shaders)
```

The renderer prefers `MaterialPass.pipeline->GetBuiltPipeline()`. If that is missing, it falls back to the renderer's own default pipeline.
