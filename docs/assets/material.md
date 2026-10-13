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

struct MaterialBinding {
    MaterialBinding(std::string_view name, MaterialParameterValue default_value);
    std::pmr::string name;
    MaterialParameterValue default_value; // required; its variant alternative defines the type
};

struct MaterialPass {
    std::pmr::string                   pass_contract;
    std::shared_ptr<RenderPipeline>    pipeline;
    std::pmr::vector<MaterialBinding>   bindings;
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

    template <MaterialParametric T> void DefineParameter(std::string_view name, T value);
    template <MaterialParametric T> void SetParameter(std::string_view name, T value);
    template <MaterialParametric T> auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;
};
```

The constructor deduplicates parameters by name (keeping the first occurrence) and takes ownership of the supplied parameter and pass descriptions.

`DefineParameter<T>(name, value)` adds a parameter and fixes its type through the active `MaterialParameterValue` variant alternative. Defining an existing name throws `std::invalid_argument`. Constructor-supplied parameters are already defined.

`SetParameter(name, value)` only assigns to an existing parameter of the same type. An undefined name or a type mismatch throws `std::invalid_argument`, leaving values, load state, and packed pass data unchanged. Successful definitions and assignments invalidate packed pass data. No separate type field is stored.

```cpp
material.DefineParameter<float>("roughness", 0.5f);
material.SetParameter("roughness", 0.8f);
```

Importers that convert a source parameter to a different runtime type construct a new parameter set before rebuilding the material; they do not use `SetParameter` to change its type.

## bindings are the GPU ABI

`MaterialParameters` is only a value table. Its storage order is **not** the shader ABI. The real order comes from each pass's `bindings`:

```text
Material.parameters              (value table, order is meaningless)
  base_color = Color(...)
  roughness  = 0.4
  albedo     = Texture(...)
  unused     = 1.0

MaterialPass("PBRForward").bindings   (ABI, order is the layout)
  base_color : Color     = White
  roughness  : Float     = 0.5
  albedo     : Texture2D = contract texture reference
        ↓
  material_data : [base_color(16B)][roughness(4B)][albedo handle(16B)]
```

A parameter that does not appear in `bindings` (such as `unused` above) is left out of that pass's `material_data`. Every binding requires a typed default supplied by the game or preset. Cook rejects missing or mistyped final parameters. Runtime warns once per material parameter revision and substitutes the contract default, preserving the expected field size and all subsequent offsets. Equal byte sizes do not imply equal types (for example, `float` versus `uint32_t`).

One material can have several passes, each with its own `bindings` and `material_data`. The renderer fetches a pass with `FindPass("the contract it agreed on")`. If the pass is missing, that material is skipped. The renderer does not infer a shading model from parameter names, and it does not synthesize a pass on the spot.

`MaterialPass` is not a RenderGraph pass. It is the binding ABI of one material for one pass contract: data, not an execution node.

## material_data packing rules

`GenerateMaterialData` resolves exactly one value per binding: a matching parameter, or that binding's mandatory default. It then encodes those values in binding order.

- Scalars, vectors, and row-major matrices follow the shared tightly packed 32-bit storage ABI.
- Textures become a 16-byte `gfx::BindlessHandle`, never a CPU pointer.
- No backend-specific 16-byte field padding is inserted. Buffer binding alignment is separate from field layout.
- `MaterialDataFields` derives shader field types, offsets, and sizes from the same binding defaults. `gfx::GenerateShaderStruct` emits the HLSL declaration.
- Contract defaults are serialized in HTGC v2. Texture defaults use asset references; runtime handles are never cooked.

`ValidateBindings()` is shared validation machinery: Cook treats its diagnostics as errors, while runtime logs warnings and continues with defaults. A binding cannot be constructed without an explicit default, so a separate expected-type tag cannot disagree with the default's type.

## Optional PBR preset

`presets/pbr` is opt-in. `hitagi_preset_pbr` owns the PBR description, defaults, `PBRForward` contract, and renderer; it depends only on Engine. `hitagi_preset_pbr_authoring` depends on that runtime preset and Editor Framework, and owns the PBR graph compiler and shader generation.

The legacy deferred renderer, concrete routing rules, and lighting passes are separately available through `hitagi_preset_deferred`. Engine starts with `ClearRenderer`, and application entry points opt into their renderer.

Engine and Editor Framework do not depend on these presets. The framework retains generic graph validation, USD import, inspectors, and Cook. The PBR game editor owns its USD-to-PBR adapter.

Frame, instance, and draw-resource declarations are generated using Engine's `gfx::ShaderField` API from CPU member types and offsets. Applications choose resource roles; Engine owns handle allocation, resource lifetime, and shader ABI machinery. The raw storage layout API is not a constant-buffer packing API.

The unfinished material shader cache, cache keys, and hot-reload toggle have been removed. This does not remove graphics shader compilation or unrelated resource registries.

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

`GenerateMaterialData` has one more fallback for textures: if a non-empty texture has no GPU view, the engine loads and uses `Texture::DefaultTexture()`. This applies to contract texture defaults as well as ordinary parameters. An explicit null texture default means no texture; the preset shader must guard sampling and define that case's behavior. The PBR preset does so.

## Load / Unload

```cpp
void Material::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && !m_HasPendingTextures) return;

    bool pending = false;
    for (auto& parameter : m_Parameters) {
        // Load parameter AND contract-default textures; mark pending if not settled
    }
    // Load the placeholder whenever a non-empty texture has no GPU view.

    for (auto& pass : m_Passes) {
        if (pass.pipeline) pass.pipeline->Load(context);
        pass.material_data = GenerateMaterialData(pass);
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

The renderer consumes `MaterialPass.pipeline->GetBuiltPipeline()`. Missing or unusable passes are handled by renderer policy; the asset layer does not invent a lighting-model fallback.
