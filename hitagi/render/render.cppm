module;

#include <spdlog/logger.h>

export module render;
export import gfx;
import std;
import utils;
import math;
import core;
import gui;
import app;
import asset;

export namespace hitagi::render {

struct TextDrawCommand {
    std::pmr::string text;
    math::vec2f      position  = {0.0f, 0.0f};
    float            font_size = 24.0f;
    math::Color      color     = math::Color::White();
};

struct RenderContext {
    gfx::Device&                    device;
    rg::RenderGraph&                graph;
    std::shared_ptr<gfx::SwapChain> swap_chain = nullptr;
};

struct RenderView {
    math::vec3f camera_position = {};
    math::mat4f view            = math::mat4f::identity();
    math::mat4f projection      = math::mat4f::identity();
};

struct RenderDrawItem {
    std::shared_ptr<asset::Mesh> mesh;
    math::mat4f                  transform = math::mat4f::identity();
    std::uint32_t                object_id = 0;
};

struct RenderLight {
    math::vec3f position;
    math::Color color     = math::Color::White();
    float       intensity = 1.0f;
};

struct RenderFrame {
    RenderView                      view;
    std::span<const RenderDrawItem> draw_items;
    std::span<const RenderLight>    lights;
};

struct RenderOutputMask {
    bool depth         = false;
    bool linear_depth  = false;
    bool object_id     = false;
    bool normal        = false;
    bool motion_vector = false;
};

struct RenderRequest {
    RenderFrame       frame;
    rg::TextureHandle target = {};
    RenderOutputMask  requested_outputs;
};

struct RenderResult {
    rg::TextureHandle color         = {};
    rg::TextureHandle depth         = {};
    rg::TextureHandle linear_depth  = {};
    rg::TextureHandle object_id     = {};
    rg::TextureHandle normal        = {};
    rg::TextureHandle motion_vector = {};
};

inline constexpr std::uint32_t MaxDeferredLights = 32;

struct DeferredLight {
    math::vec4f position_in_view;
    math::vec4f color_intensity;
};

struct FrameConstant {
    math::vec4f                                  camera_pos;
    math::mat4f                                  view;
    math::mat4f                                  projection;
    math::mat4f                                  proj_view;
    math::mat4f                                  inv_view;
    math::mat4f                                  inv_projection;
    math::mat4f                                  inv_proj_view;
    math::vec4f                                  light_position;
    math::vec4f                                  light_pos_in_view;
    math::vec3f                                  light_color;
    float                                        light_intensity;
    std::uint32_t                                light_count;
    float                                        ambient_intensity;
    float                                        exposure;
    float                                        ssao_strength;
    math::vec4f                                  viewport;
    std::array<DeferredLight, MaxDeferredLights> lights;
};

struct InstanceConstant {
    math::mat4f model;
};

struct DrawBindlessInfo {
    gfx::BindlessHandle frame_constant;
    gfx::BindlessHandle instance_constant;
    gfx::BindlessHandle material_data;
    gfx::BindlessHandle sampler;
};

enum struct RenderQueue : std::uint8_t {
    Opaque,
    Transparent,
    Debug,
};

using RenderLayerMask = std::uint32_t;

enum struct RenderLayer : RenderLayerMask {
    Default = 1u << 0u,
    Toon    = 1u << 1u,
    Outline = 1u << 2u,
    Debug   = 1u << 3u,
};

constexpr auto RenderLayerBit(RenderLayer layer) noexcept -> RenderLayerMask {
    return static_cast<RenderLayerMask>(layer);
}

enum struct MaterialPass : std::uint8_t {
    DepthPrepass,
    GBuffer,
    Forward,
    ShadowCaster,
    ToonBase,
    ToonLighting,
    ToonComposite,
    Outline,
    Debug,
};

using MaterialPassMask = std::uint32_t;

constexpr auto MaterialPassBit(MaterialPass pass) noexcept -> MaterialPassMask {
    return 1u << static_cast<std::uint32_t>(pass);
}

struct MaterialPassParticipation {
    RenderQueue      queue          = RenderQueue::Opaque;
    RenderLayerMask  layers         = RenderLayerBit(RenderLayer::Default);
    MaterialPassMask passes         = 0;
    std::int32_t     queue_priority = 0;

    constexpr auto Participates(MaterialPass pass) const noexcept -> bool {
        return (passes & MaterialPassBit(pass)) != 0;
    }

    constexpr auto IsInLayer(RenderLayer layer) const noexcept -> bool {
        return (layers & RenderLayerBit(layer)) != 0;
    }
};

struct RenderQueueKey {
    RenderQueue     queue          = RenderQueue::Opaque;
    RenderLayerMask layers         = RenderLayerBit(RenderLayer::Default);
    std::int32_t    queue_priority = 0;
    std::uint32_t   object_id      = 0;

    constexpr auto operator<=>(const RenderQueueKey& rhs) const noexcept {
        if (queue != rhs.queue) return static_cast<std::uint8_t>(queue) <=> static_cast<std::uint8_t>(rhs.queue);
        if (queue_priority != rhs.queue_priority) return queue_priority <=> rhs.queue_priority;
        if (layers != rhs.layers) return layers <=> rhs.layers;
        return object_id <=> rhs.object_id;
    }

    constexpr auto operator<(const RenderQueueKey& rhs) const noexcept -> bool {
        return (*this <=> rhs) < 0;
    }
};

struct RenderObjectIds {
    std::uint32_t object_id   = 0;
    std::uint32_t material_id = 0;
};

enum struct ToonDebugView : std::uint8_t {
    None,
    NdotL,
    RampCoordinate,
    ShadowBand,
    FaceMask,
    RimMask,
};

enum struct RenderGraphDebugView : std::uint8_t {
    Final,
    BaseColor,
    Normal,
    Metallic,
    Roughness,
    Occlusion,
    MaterialId,
    Emissive,
};

constexpr auto RenderGraphDebugViewName(RenderGraphDebugView view) noexcept -> std::string_view {
    switch (view) {
        case RenderGraphDebugView::BaseColor:
            return "albedo";
        case RenderGraphDebugView::Normal:
            return "normal";
        case RenderGraphDebugView::Metallic:
        case RenderGraphDebugView::Roughness:
        case RenderGraphDebugView::Occlusion:
            return "material";
        case RenderGraphDebugView::MaterialId:
            return "material_id";
        case RenderGraphDebugView::Emissive:
            return "emissive";
        case RenderGraphDebugView::Final:
            return "final";
    }
    return "final";
}

enum struct OutlineMode : std::uint8_t {
    Disabled,
    InvertedHull,
    ScreenSpace,
    Hybrid,
};

struct ToonRenderPathDesc {
    bool          enabled                  = false;
    bool          ramp_lighting            = true;
    bool          quantized_shadows        = true;
    bool          vertex_shadow_weight     = true;
    bool          rim_light                = true;
    bool          matcap                   = true;
    bool          emission                 = true;
    bool          face_shadow              = true;
    bool          hair_eye_special_pass    = false;
    bool          transparent_toon         = false;
    bool          post_process_color_grade = false;
    ToonDebugView debug_view               = ToonDebugView::None;
};

struct OutlineDesc {
    OutlineMode  mode                          = OutlineMode::Disabled;
    bool         use_material_width            = true;
    bool         use_material_color            = true;
    bool         use_vertex_color_width        = true;
    bool         distance_scale                = true;
    bool         transparent_outline           = false;
    float        screen_space_depth_threshold  = 0.01f;
    float        screen_space_normal_threshold = 0.2f;
    std::int32_t layer                         = 0;
    std::int32_t priority                      = 0;

    constexpr auto Enabled() const noexcept -> bool { return mode != OutlineMode::Disabled; }
};

inline auto GetMaterialPassParticipation(const asset::Material& material) noexcept -> MaterialPassParticipation {
    MaterialPassParticipation result;
    for (const auto& pass : material.GetPasses()) {
        const auto contract = std::string_view(pass.pass_contract);
        if (contract == "DepthPrepass") {
            result.passes |= MaterialPassBit(MaterialPass::DepthPrepass);
        } else if (contract == "GBuffer" || contract == "PBRGBuffer") {
            result.passes |= MaterialPassBit(MaterialPass::GBuffer);
        } else if (contract == "Forward") {
            result.passes |= MaterialPassBit(MaterialPass::Forward);
        } else if (contract == "ForwardTransparent") {
            result.queue = RenderQueue::Transparent;
            result.passes |= MaterialPassBit(MaterialPass::Forward);
        } else if (contract == "ShadowCaster") {
            result.passes |= MaterialPassBit(MaterialPass::ShadowCaster);
        } else if (contract == "ToonGBuffer") {
            result.layers |= RenderLayerBit(RenderLayer::Toon);
            result.passes |= MaterialPassBit(MaterialPass::GBuffer);
        } else if (contract == "ToonBase") {
            result.layers |= RenderLayerBit(RenderLayer::Toon);
            result.passes |= MaterialPassBit(MaterialPass::ToonBase);
        } else if (contract == "ToonLighting") {
            result.layers |= RenderLayerBit(RenderLayer::Toon);
            result.passes |= MaterialPassBit(MaterialPass::ToonLighting);
        } else if (contract == "ToonComposite") {
            result.layers |= RenderLayerBit(RenderLayer::Toon);
            result.passes |= MaterialPassBit(MaterialPass::ToonComposite);
        } else if (contract == "Outline" || contract == "OutlineMask") {
            result.layers |= RenderLayerBit(RenderLayer::Outline);
            result.passes |= MaterialPassBit(MaterialPass::Outline);
        } else if (contract == "Debug") {
            result.layers |= RenderLayerBit(RenderLayer::Debug);
            result.queue = RenderQueue::Debug;
            result.passes |= MaterialPassBit(MaterialPass::Debug);
        }
    }
    return result;
}

inline auto FindMaterialPassForRenderPass(const asset::Material& material, MaterialPass render_pass) noexcept -> const asset::MaterialPass* {
    for (const auto& pass : material.GetPasses()) {
        const auto contract = std::string_view(pass.pass_contract);
        switch (render_pass) {
            case MaterialPass::DepthPrepass:
                if (contract == "DepthPrepass") return std::addressof(pass);
                break;
            case MaterialPass::GBuffer:
                if (contract == "GBuffer" || contract == "PBRGBuffer" || contract == "ToonGBuffer") return std::addressof(pass);
                break;
            case MaterialPass::Forward:
                if (contract == "Forward" || contract == "ForwardTransparent") return std::addressof(pass);
                break;
            case MaterialPass::ShadowCaster:
                if (contract == "ShadowCaster") return std::addressof(pass);
                break;
            case MaterialPass::ToonBase:
                if (contract == "ToonBase") return std::addressof(pass);
                break;
            case MaterialPass::ToonLighting:
                if (contract == "ToonLighting") return std::addressof(pass);
                break;
            case MaterialPass::ToonComposite:
                if (contract == "ToonComposite") return std::addressof(pass);
                break;
            case MaterialPass::Outline:
                if (contract == "Outline" || contract == "OutlineMask") return std::addressof(pass);
                break;
            case MaterialPass::Debug:
                if (contract == "Debug") return std::addressof(pass);
                break;
        }
    }
    return nullptr;
}

constexpr auto MakeDefaultToonRenderPathDesc() noexcept -> ToonRenderPathDesc {
    return ToonRenderPathDesc{.enabled = true};
}

constexpr auto MakeDefaultToonOutlineDesc() noexcept -> OutlineDesc {
    return OutlineDesc{.mode = OutlineMode::Hybrid};
}

struct MaterialInfo {
    std::shared_ptr<asset::Material> material;
    std::pmr::string                 material_pass_contract;
    std::shared_ptr<gfx::RenderPipeline> pipeline;
    rg::GPUBufferHandle              material_data;
    MaterialPassParticipation        pass_participation;
};

struct MaterialTextureInfo {
    std::shared_ptr<asset::Material> material;
    std::pmr::string                 material_pass_contract;
};

struct MeshInfo {
    std::shared_ptr<asset::Mesh>                                  mesh;
    utils::EnumArray<rg::GPUBufferHandle, asset::VertexAttribute> vertices;
    rg::GPUBufferHandle                                           indices;
    gfx::Format                                                   index_format = gfx::Format::R32_UINT;
};

struct InstanceInfo {
    std::shared_ptr<asset::Mesh> mesh;
    InstanceConstant             instance_data;
    std::size_t                  instance_index;
    std::uint32_t                object_id = 0;
};

struct RenderDrawState {
    std::pmr::unordered_map<asset::Material*, MaterialInfo>             material_infos;
    std::pmr::unordered_map<asset::Material*, MaterialTextureInfo>      material_texture_infos;
    std::pmr::unordered_set<asset::Material*>                           active_materials;
    std::pmr::unordered_map<asset::Mesh*, MeshInfo>                     mesh_infos;
    std::pmr::vector<InstanceInfo>                                      instance_infos;

    void ClearFrame() {
        material_infos.clear();
        active_materials.clear();
        instance_infos.clear();
    }

    void InvalidateResources() {
        material_infos.clear();
        material_texture_infos.clear();
        active_materials.clear();
        mesh_infos.clear();
        instance_infos.clear();
    }
};

class IRenderer : public core::RuntimeModule {
public:
    using core::RuntimeModule::RuntimeModule;

    virtual ~IRenderer() = default;

    virtual auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult = 0;
};

class GuiRenderUtils {
public:
    GuiRenderUtils(gfx::Device& gfx_device);

    void GuiPass(rg::RenderGraph& render_graph, rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target);

protected:
    // GUI render data
    struct GuiRenderData {
        std::shared_ptr<gfx::Shader>         vs, ps;
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        std::shared_ptr<gfx::Texture>        font_texture;
        std::shared_ptr<gfx::Sampler>        sampler;
    } m_GfxData;

    rg::TextureHandle m_FontTexture;
    std::uint64_t     m_FontTextureGeneration = 0;
};

class TextRenderUtils {
public:
    TextRenderUtils(gfx::Device& gfx_device, std::filesystem::path font_dir);
    ~TextRenderUtils();

    TextRenderUtils(const TextRenderUtils&)            = delete;
    TextRenderUtils& operator=(const TextRenderUtils&) = delete;
    TextRenderUtils(TextRenderUtils&&)                 = delete;
    TextRenderUtils& operator=(TextRenderUtils&&)      = delete;

    void TextPass(rg::RenderGraph& render_graph, rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

struct DeferredRenderResources {
    rg::TextureHandle   color;
    rg::TextureHandle   depth;
    rg::TextureHandle   linear_depth;
    rg::TextureHandle   object_id;
    rg::TextureHandle   material_id;
    rg::TextureHandle   gbuffer_albedo;
    rg::TextureHandle   gbuffer_normal;
    rg::TextureHandle   gbuffer_material;
    rg::TextureHandle   gbuffer_emissive;
    rg::TextureHandle   shadow_map;
    rg::GPUBufferHandle frame_constant;
    rg::SamplerHandle   sampler;
    std::uint32_t       width  = 0;
    std::uint32_t       height = 0;
};

struct DeferredDrawData {
    std::span<const InstanceInfo> instances;
    const RenderDrawState*        draw_state = nullptr;
};

class IDeferredRenderExtension {
public:
    virtual ~IDeferredRenderExtension() = default;

    virtual void AfterGBuffer(
        RenderContext&                 context,
        const RenderView&              view,
        const DeferredRenderResources& resources,
        const DeferredDrawData&        draw_data) {}

    virtual void AfterLighting(
        RenderContext&           context,
        const RenderView&        view,
        DeferredRenderResources& resources,
        const DeferredDrawData&  draw_data) {}
};

// Shader source text together with the path it was read from. Passes receive
// the text directly so they never touch the file system; the path is kept for
// compiler diagnostics and relative #include resolution.
struct ShaderSource {
    std::filesystem::path path;
    std::pmr::string      code;
};

auto LoadShaderSource(core::FileIOManager& file_io, std::filesystem::path path) -> ShaderSource;

namespace passes {

using Gui  = GuiRenderUtils;
using Text = TextRenderUtils;

class Present {
public:
    void Build(RenderContext& context, rg::TextureHandle input);
};

struct GBufferOutput {
    rg::TextureHandle albedo;
    rg::TextureHandle normal;
    rg::TextureHandle material;
    rg::TextureHandle emissive;
    rg::TextureHandle object_material_id;
    rg::TextureHandle depth;
};

class DepthPrepass {
public:
    struct Desc {
        std::uint32_t width  = 1;
        std::uint32_t height = 1;
        gfx::Format   format = gfx::Format::D32_FLOAT;
    };

    struct BuildDesc {
        std::pmr::string         pass_name;
        rg::TextureHandle        depth;
        rg::GPUBufferHandle      frame_constant;
        rg::GPUBufferHandle      instance_constant;
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        bool                     clear_depth = true;
        std::uint32_t            width       = 1;
        std::uint32_t            height      = 1;
    };

    static auto CreateTarget(RenderContext& context, const Desc& desc) -> rg::TextureHandle;
    static void Build(RenderContext& context, const BuildDesc& desc);
};

class ShadowMapPass {
public:
    struct Desc {
        std::uint32_t width  = 1024;
        std::uint32_t height = 1024;
        gfx::Format   format = gfx::Format::D32_FLOAT;
    };

    struct BuildDesc {
        std::pmr::string         pass_name;
        rg::TextureHandle        shadow_map;
        rg::GPUBufferHandle      light_frame_constant;
        rg::GPUBufferHandle      instance_constant;
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        bool                     clear_depth = true;
        std::uint32_t            width       = 1024;
        std::uint32_t            height      = 1024;
    };

    static auto CreateTarget(RenderContext& context, const Desc& desc) -> rg::TextureHandle;
    static void Build(RenderContext& context, const BuildDesc& desc);
};

class ObjectMaterialIdPass {
public:
    struct Desc {
        std::uint32_t width  = 1;
        std::uint32_t height = 1;
        gfx::Format   format = gfx::Format::R32G32_UINT;
    };

    static auto CreateTarget(RenderContext& context, const Desc& desc) -> rg::TextureHandle;
};

class GBuffer {
public:
    GBuffer(gfx::Device& device, ShaderSource shader);

    struct Desc {
        std::uint32_t width                     = 1;
        std::uint32_t height                    = 1;
        gfx::Format   albedo_format             = gfx::Format::R32G32B32A32_FLOAT;
        gfx::Format   normal_format             = gfx::Format::R8G8B8A8_UNORM;
        gfx::Format   material_format           = gfx::Format::R8G8B8A8_UNORM;
        gfx::Format   emissive_format           = gfx::Format::R11G11B10_FLOAT;
        gfx::Format   object_material_id_format = gfx::Format::R32G32_UINT;
    };

    struct AttributePassDesc {
        std::pmr::string         pass_name;
        rg::TextureHandle        target;
        rg::TextureHandle        depth;
        rg::TextureHandle        dependency;
        rg::GPUBufferHandle      frame_constant;
        rg::GPUBufferHandle      instance_constant;
        rg::GPUBufferHandle      bindless_info;
        rg::SamplerHandle        sampler;
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        bool                     clear_depth = false;
        std::uint32_t            width       = 1;
        std::uint32_t            height      = 1;
    };

    struct AlbedoPassDesc {
        std::pmr::string    pass_name;
        rg::TextureHandle   target;
        rg::TextureHandle   depth;
        FrameConstant       frame_constant;
        rg::GPUBufferHandle frame_constant_buffer;
        rg::GPUBufferHandle instance_constant_buffer;
        rg::GPUBufferHandle bindless_info_buffer;
        rg::SamplerHandle   sampler;
        gfx::Device::Type   device_type = gfx::Device::Type::Mock;
        bool                clear_depth = true;
        std::uint32_t       width       = 1;
        std::uint32_t       height      = 1;
    };

    auto CreateTargets(RenderContext& context, const Desc& desc) -> GBufferOutput;
    auto GetAlbedoPipeline() -> std::shared_ptr<gfx::RenderPipeline>;
    auto GetNormalPipeline() -> std::shared_ptr<gfx::RenderPipeline>;
    auto GetMaterialPipeline() -> std::shared_ptr<gfx::RenderPipeline>;
    auto GetEmissivePipeline() -> std::shared_ptr<gfx::RenderPipeline>;
    void BuildAlbedoPass(RenderContext& context, RenderDrawState& draw_state, const AlbedoPassDesc& desc);
    void BuildAttributePass(RenderContext& context, RenderDrawState& draw_state, const AttributePassDesc& desc);

private:
    void EnsureResources();

    gfx::Device&                         m_Device;
    ShaderSource                         m_Shader;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_AlbedoPS;
    std::shared_ptr<gfx::Shader>         m_NormalPS;
    std::shared_ptr<gfx::Shader>         m_MaterialPS;
    std::shared_ptr<gfx::Shader>         m_EmissivePS;
    std::shared_ptr<gfx::RenderPipeline> m_AlbedoPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_NormalPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_MaterialPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_EmissivePipeline;
};

class DeferredLighting {
public:
    DeferredLighting(gfx::Device& device, ShaderSource shader);

    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle gbuffer_albedo;
        gfx::BindlessHandle gbuffer_normal;
        gfx::BindlessHandle gbuffer_material;
        gfx::BindlessHandle gbuffer_emissive;
        gfx::BindlessHandle sampler;
    };

    auto GetPipeline(gfx::Format target_format) -> std::shared_ptr<gfx::RenderPipeline>;

    auto Build(
        RenderContext&           context,
        const GBufferOutput&     gbuffer,
        rg::GPUBufferHandle      frame_constant,
        rg::GPUBufferHandle      bindless_info,
        rg::SamplerHandle        sampler,
        std::shared_ptr<gfx::RenderPipeline> pipeline,
        rg::TextureHandle        target) -> rg::TextureHandle;

private:
    void EnsureResources(gfx::Format target_format);

    gfx::Device&                         m_Device;
    ShaderSource                         m_Shader;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_PS;
    std::shared_ptr<gfx::RenderPipeline> m_Pipeline;
    gfx::Format                          m_TargetFormat = gfx::Format::UNKNOWN;
};

class GBufferDebugView {
public:
    GBufferDebugView(gfx::Device& device, ShaderSource shader);

    auto Build(
        RenderContext&       context,
        const GBufferOutput& gbuffer,
        rg::SamplerHandle    sampler,
        std::string_view     view_name,
        rg::TextureHandle    target) -> rg::TextureHandle;

private:
    void EnsureResources(gfx::Format target_format);

    struct BindlessInfo {
        gfx::BindlessHandle gbuffer_albedo;
        gfx::BindlessHandle gbuffer_normal;
        gfx::BindlessHandle gbuffer_material;
        gfx::BindlessHandle gbuffer_emissive;
        gfx::BindlessHandle object_material_id;
        gfx::BindlessHandle sampler;
    };

    gfx::Device&                         m_Device;
    ShaderSource                         m_Shader;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_AlbedoPS;
    std::shared_ptr<gfx::Shader>         m_NormalPS;
    std::shared_ptr<gfx::Shader>         m_MaterialPS;
    std::shared_ptr<gfx::Shader>         m_EmissivePS;
    std::shared_ptr<gfx::Shader>         m_ObjectMaterialIdPS;
    std::shared_ptr<gfx::RenderPipeline> m_AlbedoPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_NormalPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_MaterialPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_EmissivePipeline;
    std::shared_ptr<gfx::RenderPipeline> m_ObjectMaterialIdPipeline;
    gfx::Format                          m_TargetFormat = gfx::Format::UNKNOWN;
};

}  // namespace passes

class RenderRuntime : public core::RuntimeModule {
public:
    RenderRuntime(gfx::Device& device, const Application& app, std::string_view name = "");

    void Tick() override;

    auto MakeContext() noexcept -> RenderContext;

    void RenderGui(rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target);

    void RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target = false);

    void CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer = {}, gfx::TextureSubresourceLayer to_layer = {});
    void CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer = {});

    void ToSwapChain(rg::TextureHandle from);

    inline auto GetFrameTime() const noexcept -> std::chrono::duration<double> { return m_Clock.DeltaTime(); }

    inline auto GetRenderGraph() noexcept -> rg::RenderGraph& { return m_RenderGraph; }

    auto GetSwapChain() const noexcept -> gfx::SwapChain& { return *m_SwapChain; }

    auto GetSwapChainPtr() const noexcept -> std::shared_ptr<gfx::SwapChain> { return m_SwapChain; }

private:
    const Application&               m_App;
    gfx::Device&                     m_GfxDevice;
    std::shared_ptr<gfx::SwapChain>  m_SwapChain;
    rg::RenderGraph                  m_RenderGraph;
    std::unique_ptr<GuiRenderUtils>  m_GuiRenderUtils;
    std::unique_ptr<TextRenderUtils> m_TextRenderUtils;
    passes::Present                  m_PresentPass;
    rg::TextureHandle                m_GuiTarget;
    const gui::GuiDrawData*          m_GuiDrawData    = nullptr;
    bool                             m_ClearGuiTarget = false;
    core::Clock                      m_Clock;
};

class DeferredRenderer : public IRenderer {
public:
    // Shader sources are read through `file_io` during construction only; the
    // renderer keeps no reference to it afterwards.
    DeferredRenderer(gfx::Device& device, core::FileIOManager& file_io, const Application& app, std::string_view name = "");

    auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult override;
    void AddExtension(std::shared_ptr<IDeferredRenderExtension> extension);
    void ClearExtensions();

private:
    auto RenderFrame(RenderContext& context, const RenderRequest& request) -> RenderResult;
    void RecordMaterial(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Material>& material);
    void RecordMesh(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Mesh>& mesh);
    void RecordInstance(rg::RenderGraph& render_graph, const RenderDrawItem& item);
    // this function must invoke after all instance are finished, it will:
    // 1. update index of material in the constant buffer of material,
    // 2. create constant buffer of materials
    // 3. create constant buffer of instances
    // 4. create constant buffer of frame
    // 5. create constant buffer of bindless info
    void UpdateConstantBuffer(rg::RenderGraph& render_graph, std::shared_ptr<gfx::RenderPipeline> default_pipeline);
    void ClearFrameState();

    const Application& m_App;
    gfx::Device&       m_GfxDevice;

    // Execution environment handed to every asset Load() call in this renderer.
    asset::ResourceLoadContext m_LoadContext;

    std::shared_ptr<gfx::Sampler> m_PersistentSampler;

    passes::GBuffer                                             m_GBufferPass;
    passes::DeferredLighting                                    m_DeferredLightingPass;
    passes::GBufferDebugView                                    m_GBufferDebugViewPass;
    std::pmr::vector<std::shared_ptr<IDeferredRenderExtension>> m_Extensions;

    // frame state
    rg::SamplerHandle   m_Sampler;
    rg::GPUBufferHandle m_FrameConstantBuffer;
    rg::GPUBufferHandle m_InstanceConstantBuffer;
    rg::GPUBufferHandle m_BindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_NormalBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_MaterialBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_EmissiveBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_DeferredLightingBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_GBufferDebugViewBindlessInfoConstantBuffer;

    RenderDrawState m_DrawState;
};

using DefaultRenderer = DeferredRenderer;

}  // namespace hitagi::render
