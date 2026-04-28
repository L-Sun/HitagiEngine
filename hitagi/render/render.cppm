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

class IRenderer : public core::RuntimeModule {
public:
    using core::RuntimeModule::RuntimeModule;

    virtual ~IRenderer() = default;

    // Render scene to texture
    virtual void RenderScene(std::shared_ptr<asset::Scene> scene, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) = 0;

    virtual void RenderGui(rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target) = 0;

    virtual void RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target = false) = 0;

    virtual void CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer = {}, gfx::TextureSubresourceLayer to_layer = {}) = 0;
    virtual void CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer = {})                                            = 0;

    virtual void ToSwapChain(rg::TextureHandle from) = 0;

    // Get frame time
    virtual auto GetFrameTime() const noexcept -> std::chrono::duration<double> = 0;

    // Get the render graph
    virtual auto GetRenderGraph() noexcept -> rg::RenderGraph& = 0;

    virtual auto GetSwapChain() const noexcept -> gfx::SwapChain& = 0;
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

class ForwardRenderer : public IRenderer {
public:
    ForwardRenderer(gfx::Device& device, const Application& app, std::string_view name = "");

    void Tick() override;

    void RenderScene(std::shared_ptr<asset::Scene> scene, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) override;

    void RenderGui(rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target) override;

    void RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target = false) override;

    void CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer = {}, gfx::TextureSubresourceLayer to_layer = {}) override;
    void CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer = {}) override;

    void ToSwapChain(rg::TextureHandle from) override;

    inline auto GetFrameTime() const noexcept -> std::chrono::duration<double> override { return m_Clock.DeltaTime(); }

    inline auto GetRenderGraph() noexcept -> rg::RenderGraph& override { return m_RenderGraph; }

    auto GetSwapChain() const noexcept -> gfx::SwapChain& final { return *m_SwapChain; }

private:
    struct FrameConstant {
        math::vec4f camera_pos;
        math::mat4f view;
        math::mat4f projection;
        math::mat4f proj_view;
        math::mat4f inv_view;
        math::mat4f inv_projection;
        math::mat4f inv_proj_view;
        math::vec4f light_position;
        math::vec4f light_pos_in_view;
        math::vec3f light_color;
        float       light_intensity;
    };
    struct InstanceConstant {
        math::mat4f model;
    };

    struct BindlessInfo {
        gfx::BindlessHandle                frame_constant;
        gfx::BindlessHandle                instance_constant;
        gfx::BindlessHandle                material_constant;
        std::array<gfx::BindlessHandle, 4> textures;
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
        std::shared_ptr<asset::Mesh> mesh;
        InstanceConstant             instance_data;
        std::size_t                  instance_index;
    };

    void RecordMaterialInstance(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::MaterialInstance>& material_instance);
    void RecordMesh(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::Mesh>& mesh);
    void RecordInstance(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::Mesh>& mesh, math::mat4f transform);
    // this function must invoke after all instance are finished, it will:
    // 1. update index of material_instance in the constant buffer of material,
    // 2. create constant buffer of materials
    // 3. create constant buffer of instances
    // 4. create constant buffer of frame
    // 5. create constant buffer of bindless info
    void UpdateConstantBuffer(rg::RenderPassBuilder& builder);
    void ClearFrameState();
    void InvalidateSceneCaches();

    const Application& m_App;
    gfx::Device&       m_GfxDevice;

    core::Clock m_Clock;

    std::shared_ptr<gfx::SwapChain> m_SwapChain;
    std::shared_ptr<gfx::Sampler>   m_PersistentSampler;
    rg::RenderGraph                 m_RenderGraph;

    std::unique_ptr<GuiRenderUtils>  m_GuiRenderUtils;
    std::unique_ptr<TextRenderUtils> m_TextRenderUtils;
    rg::TextureHandle                m_GuiTarget;
    const gui::GuiDrawData*          m_GuiDrawData    = nullptr;
    bool                             m_ClearGuiTarget = false;

    // frame state
    rg::SamplerHandle   m_Sampler;
    rg::GPUBufferHandle m_FrameConstantBuffer;
    rg::GPUBufferHandle m_InstanceConstantBuffer;
    rg::GPUBufferHandle m_BindlessInfoConstantBuffer;
    asset::Scene*       m_CachedScene = nullptr;

    std::pmr::unordered_map<asset::Material*, MaterialInfo>                 m_MaterialInfos;
    std::pmr::unordered_map<asset::Material*, rg::RenderPipelineHandle>     m_PipelineHandles;
    std::pmr::unordered_map<asset::MaterialInstance*, MaterialInstanceInfo> m_MaterialInstanceInfos;
    std::pmr::unordered_map<asset::MaterialInstance*, std::size_t>          m_MaterialInstanceIndices;
    std::pmr::unordered_set<asset::MaterialInstance*>                       m_ActiveMaterialInstances;
    std::pmr::unordered_map<asset::Mesh*, MeshInfo>                         m_MeshInfos;
    std::pmr::vector<InstanceInfo>                                          m_InstanceInfos;
};

}  // namespace hitagi::render
