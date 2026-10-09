export module render:types;
import std;
import utils;
import math;
import core;
import gfx;
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

struct RenderRequest {
    RenderFrame       frame;
    rg::TextureHandle target = {};
    RenderOutputMask  requested_outputs;
    // Selects the deferred renderer output for this call only.
    RenderGraphDebugView debug_view = RenderGraphDebugView::Final;
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
    std::uint32_t       instance_index  = 0;
    std::uint32_t       instance_stride = 0;
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
    std::shared_ptr<asset::Material>     material;
    std::pmr::string                     material_pass_contract;
    std::shared_ptr<gfx::RenderPipeline> pipeline;
    rg::GPUBufferHandle                  material_data;
    MaterialPassParticipation            pass_participation;
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
    std::pmr::unordered_map<asset::Material*, MaterialInfo>        material_infos;
    std::pmr::unordered_map<asset::Material*, MaterialTextureInfo> material_texture_infos;
    std::pmr::unordered_set<asset::Material*>                      active_materials;
    std::pmr::unordered_map<asset::Mesh*, MeshInfo>                mesh_infos;
    std::pmr::vector<InstanceInfo>                                 instance_infos;

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

// Shader source text together with the path it was read from. Passes receive
// the text directly so they never touch the file system; the path is kept for
// compiler diagnostics and relative #include resolution.
struct ShaderSource {
    std::filesystem::path path;
    std::pmr::string      code;
};

auto LoadShaderSource(core::FileIOManager& file_io, std::filesystem::path path) -> ShaderSource;

}  // namespace hitagi::render
