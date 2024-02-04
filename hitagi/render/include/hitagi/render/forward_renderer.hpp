#pragma once
#include <hitagi/render/renderer.hpp>
#include <hitagi/render/utils.hpp>
#include <hitagi/application.hpp>
#include <hitagi/render_graph/render_graph.hpp>

#include <unordered_map>

namespace hitagi::render {

class ForwardRenderer : public IRenderer {
public:
    ForwardRenderer(gfx::Device& device, const Application& app, gui::GuiManager* gui_manager = nullptr, std::string_view name = "");

    void Tick() override;

    void RenderScene(std::shared_ptr<asset::Scene> scene, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) override;

    void RenderGui(rg::TextureHandle target, bool clear_target) override;

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
        std::pmr::vector<rg::SamplerHandle>      samplers;
        std::size_t                              material_instance_index;
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

    const Application& m_App;
    gfx::Device&       m_GfxDevice;

    core::Clock m_Clock;

    std::shared_ptr<gfx::SwapChain> m_SwapChain;
    rg::RenderGraph                 m_RenderGraph;

    std::unique_ptr<GuiRenderUtils> m_GuiRenderUtils;
    rg::TextureHandle               m_GuiTarget;
    bool                            m_ClearGuiTarget = false;

    // frame state
    rg::SamplerHandle   m_Sampler;
    rg::GPUBufferHandle m_FrameConstantBuffer;
    rg::GPUBufferHandle m_InstanceConstantBuffer;
    rg::GPUBufferHandle m_BindlessInfoConstantBuffer;

    std::pmr::unordered_map<asset::Material*, MaterialInfo>                 m_MaterialInfos;
    std::pmr::unordered_map<asset::MaterialInstance*, MaterialInstanceInfo> m_MaterialInstanceInfos;
    std::pmr::unordered_map<asset::Mesh*, MeshInfo>                         m_MeshInfos;
    std::pmr::vector<InstanceInfo>                                          m_InstanceInfos;
};

}  // namespace hitagi::render