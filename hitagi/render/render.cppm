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
import ecs;

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

struct SceneView {
    std::shared_ptr<asset::Scene> scene;
    const asset::Camera*          camera = nullptr;
    math::mat4f                   camera_transform;
};

struct RenderOutputMask {
    bool depth         = false;
    bool linear_depth  = false;
    bool object_id     = false;
    bool normal        = false;
    bool motion_vector = false;
};

struct RenderRequest {
    SceneView        view;
    rg::TextureHandle target = {};
    RenderOutputMask requested_outputs;
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

struct SceneFrameConstant {
    math::vec4f camera_pos;
    math::mat4f view;
    math::mat4f projection;
    math::mat4f proj_view;
    math::mat4f inv_view;
    math::mat4f inv_projection;
    math::mat4f inv_proj_view;
    math::vec4f   light_position;
    math::vec4f   light_pos_in_view;
    math::vec3f   light_color;
    float         light_intensity;
    std::uint32_t light_count;
    float         ambient_intensity;
    float         exposure;
    float         ssao_strength;
    math::vec4f   viewport;
    std::array<DeferredLight, MaxDeferredLights> lights;
};

struct SceneInstanceConstant {
    math::mat4f model;
};

struct DrawBindlessInfo {
    gfx::BindlessHandle                frame_constant;
    gfx::BindlessHandle                instance_constant;
    gfx::BindlessHandle                material_constant;
    std::array<gfx::BindlessHandle, 7> textures;
    gfx::BindlessHandle                sampler;
};

struct MaterialInfo {
    std::shared_ptr<asset::Material> material;
    rg::RenderPipelineHandle         pipeline;
    rg::GPUBufferHandle              material_constant;
};

struct MaterialInstanceInfo {
    std::shared_ptr<asset::MaterialInstance> material_instance;
    std::pmr::vector<rg::TextureHandle>      textures;
};

struct MeshInfo {
    std::shared_ptr<asset::Mesh>                                  mesh;
    utils::EnumArray<rg::GPUBufferHandle, asset::VertexAttribute> vertices;
    rg::GPUBufferHandle                                           indices;
};

struct InstanceInfo {
    ecs::Entity                  entity;
    std::shared_ptr<asset::Mesh> mesh;
    SceneInstanceConstant        instance_data;
    std::size_t                  instance_index;
};

struct SceneDrawState {
    std::pmr::unordered_map<asset::Material*, MaterialInfo>                 material_infos;
    std::pmr::unordered_map<asset::Material*, rg::RenderPipelineHandle>     pipeline_handles;
    std::pmr::unordered_map<asset::MaterialInstance*, MaterialInstanceInfo> material_instance_infos;
    std::pmr::unordered_map<asset::MaterialInstance*, std::size_t>          material_instance_indices;
    std::pmr::unordered_set<asset::MaterialInstance*>                       active_material_instances;
    std::pmr::unordered_map<asset::Mesh*, MeshInfo>                         mesh_infos;
    std::pmr::vector<InstanceInfo>                                          instance_infos;

    void ClearFrame() {
        material_infos.clear();
        material_instance_indices.clear();
        active_material_instances.clear();
        instance_infos.clear();
    }

    void InvalidateScene() {
        material_infos.clear();
        pipeline_handles.clear();
        material_instance_infos.clear();
        material_instance_indices.clear();
        active_material_instances.clear();
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
    rg::TextureHandle color;
    rg::TextureHandle depth;
    rg::TextureHandle linear_depth;
    rg::TextureHandle gbuffer_albedo;
    rg::TextureHandle gbuffer_normal;
    rg::TextureHandle gbuffer_material;
    rg::TextureHandle gbuffer_emissive;
    rg::GPUBufferHandle frame_constant;
    rg::SamplerHandle sampler;
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
};

struct DeferredSceneDrawData {
    std::span<const InstanceInfo> instances;
    const SceneDrawState*         scene_draw_state = nullptr;
};

class IDeferredRenderExtension {
public:
    virtual ~IDeferredRenderExtension() = default;

    virtual void AfterGBuffer(
        RenderContext&                  context,
        const SceneView&                view,
        const DeferredRenderResources&  resources,
        const DeferredSceneDrawData&    draw_data) {}

    virtual void AfterLighting(
        RenderContext&                  context,
        const SceneView&                view,
        DeferredRenderResources&        resources,
        const DeferredSceneDrawData&    draw_data) {}
};

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
    rg::TextureHandle depth;
};

class GBuffer {
public:
    GBuffer(gfx::Device& device, std::filesystem::path shader_path);

    struct Desc {
        std::uint32_t width  = 1;
        std::uint32_t height = 1;
        gfx::Format   albedo_format   = gfx::Format::R32G32B32A32_FLOAT;
        gfx::Format   normal_format   = gfx::Format::R8G8B8A8_UNORM;
        gfx::Format   material_format = gfx::Format::R8G8B8A8_UNORM;
        gfx::Format   emissive_format = gfx::Format::R11G11B10_FLOAT;
    };

    struct AttributePassDesc {
        std::pmr::string        pass_name;
        rg::TextureHandle       target;
        rg::TextureHandle       depth;
        rg::TextureHandle       dependency;
        rg::GPUBufferHandle     frame_constant;
        rg::GPUBufferHandle     instance_constant;
        rg::GPUBufferHandle     bindless_info;
        rg::SamplerHandle       sampler;
        rg::RenderPipelineHandle pipeline;
        bool                    clear_depth = false;
        std::uint32_t           width  = 1;
        std::uint32_t           height = 1;
    };

    struct AlbedoPassDesc {
        std::pmr::string         pass_name;
        rg::TextureHandle        target;
        rg::TextureHandle        depth;
        SceneFrameConstant       frame_constant;
        rg::GPUBufferHandle      frame_constant_buffer;
        rg::GPUBufferHandle      instance_constant_buffer;
        rg::GPUBufferHandle      bindless_info_buffer;
        rg::SamplerHandle        sampler;
        gfx::Device::Type        device_type = gfx::Device::Type::Mock;
        bool                     clear_depth = true;
        std::uint32_t            width       = 1;
        std::uint32_t            height      = 1;
    };

    auto CreateTargets(RenderContext& context, const Desc& desc) -> GBufferOutput;
    auto ImportAlbedoPipeline(RenderContext& context, std::string_view name = {}) -> rg::RenderPipelineHandle;
    auto ImportNormalPipeline(RenderContext& context, std::string_view name = {}) -> rg::RenderPipelineHandle;
    auto ImportMaterialPipeline(RenderContext& context, std::string_view name = {}) -> rg::RenderPipelineHandle;
    auto ImportEmissivePipeline(RenderContext& context, std::string_view name = {}) -> rg::RenderPipelineHandle;
    void BuildAlbedoPass(RenderContext& context, SceneDrawState& draw_state, const AlbedoPassDesc& desc);
    void BuildAttributePass(RenderContext& context, SceneDrawState& draw_state, const AttributePassDesc& desc);

private:
    void EnsureResources();

    gfx::Device&                         m_Device;
    std::filesystem::path                m_ShaderPath;
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
    DeferredLighting(gfx::Device& device, std::filesystem::path shader_path);

    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle gbuffer_albedo;
        gfx::BindlessHandle gbuffer_normal;
        gfx::BindlessHandle gbuffer_material;
        gfx::BindlessHandle gbuffer_emissive;
        gfx::BindlessHandle sampler;
    };

    auto ImportPipeline(RenderContext& context, gfx::Format target_format, std::string_view name = {}) -> rg::RenderPipelineHandle;

    auto Build(
        RenderContext&            context,
        const GBufferOutput&      gbuffer,
        rg::GPUBufferHandle       frame_constant,
        rg::GPUBufferHandle       bindless_info,
        rg::SamplerHandle         sampler,
        rg::RenderPipelineHandle  pipeline,
        rg::TextureHandle         target) -> rg::TextureHandle;

private:
    void EnsureResources(gfx::Format target_format);

    gfx::Device&                         m_Device;
    std::filesystem::path                m_ShaderPath;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_PS;
    std::shared_ptr<gfx::RenderPipeline> m_Pipeline;
    gfx::Format                          m_TargetFormat = gfx::Format::UNKNOWN;
};

class GBufferDebugView {
public:
    GBufferDebugView(gfx::Device& device, std::filesystem::path shader_path);

    auto Build(
        RenderContext&            context,
        const GBufferOutput&      gbuffer,
        rg::SamplerHandle         sampler,
        std::string_view          view_name,
        rg::TextureHandle         target) -> rg::TextureHandle;

private:
    void EnsureResources(gfx::Format target_format);

    struct BindlessInfo {
        gfx::BindlessHandle gbuffer_albedo;
        gfx::BindlessHandle gbuffer_normal;
        gfx::BindlessHandle gbuffer_material;
        gfx::BindlessHandle gbuffer_emissive;
        gfx::BindlessHandle sampler;
    };

    gfx::Device&                         m_Device;
    std::filesystem::path                m_ShaderPath;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_AlbedoPS;
    std::shared_ptr<gfx::Shader>         m_NormalPS;
    std::shared_ptr<gfx::Shader>         m_MaterialPS;
    std::shared_ptr<gfx::Shader>         m_EmissivePS;
    std::shared_ptr<gfx::RenderPipeline> m_AlbedoPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_NormalPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_MaterialPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_EmissivePipeline;
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
    const Application&              m_App;
    gfx::Device&                    m_GfxDevice;
    std::shared_ptr<gfx::SwapChain> m_SwapChain;
    rg::RenderGraph                 m_RenderGraph;
    std::unique_ptr<GuiRenderUtils>  m_GuiRenderUtils;
    std::unique_ptr<TextRenderUtils> m_TextRenderUtils;
    passes::Present                 m_PresentPass;
    rg::TextureHandle               m_GuiTarget;
    const gui::GuiDrawData*         m_GuiDrawData    = nullptr;
    bool                            m_ClearGuiTarget = false;
    core::Clock                     m_Clock;
};

class DeferredRenderer : public IRenderer {
public:
    DeferredRenderer(gfx::Device& device, const Application& app, std::string_view name = "");

    auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult override;
    void AddExtension(std::shared_ptr<IDeferredRenderExtension> extension);
    void ClearExtensions();

private:
    auto RenderScene(RenderContext& context, const RenderRequest& request) -> RenderResult;
    void RecordMaterialInstance(rg::RenderGraph& render_graph, const std::shared_ptr<asset::MaterialInstance>& material_instance);
    void RecordMesh(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Mesh>& mesh);
    void RecordInstance(rg::RenderGraph& render_graph, ecs::Entity entity, const std::shared_ptr<asset::Mesh>& mesh, math::mat4f transform);
    // this function must invoke after all instance are finished, it will:
    // 1. update index of material_instance in the constant buffer of material,
    // 2. create constant buffer of materials
    // 3. create constant buffer of instances
    // 4. create constant buffer of frame
    // 5. create constant buffer of bindless info
    void UpdateConstantBuffer(rg::RenderGraph& render_graph, rg::RenderPipelineHandle albedo_pipeline);
    void ClearFrameState();

    const Application& m_App;
    gfx::Device&       m_GfxDevice;

    std::shared_ptr<gfx::Sampler>        m_PersistentSampler;

    passes::GBuffer                  m_GBufferPass;
    passes::DeferredLighting         m_DeferredLightingPass;
    passes::GBufferDebugView         m_GBufferDebugViewPass;
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
    asset::Scene*       m_CachedScene = nullptr;

    SceneDrawState m_SceneDrawState;
};

using DefaultRenderer = DeferredRenderer;

}  // namespace hitagi::render
