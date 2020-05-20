#pragma once
#include "GraphicsManager.hpp"
#include "D3DCore.hpp"
#include "FrameResource.hpp"
#include "CommandListManager.hpp"
#include "CommandContext.hpp"
#include "GpuBuffer.hpp"
#include "RootSignature.hpp"
#include "DescriptorAllocator.hpp"
#include "PipeLineState.hpp"
#include "ColorBuffer.hpp"
#include "DepthBuffer.hpp"

namespace Hitagi::Graphics::DX12 {

class D3D12GraphicsManager : public GraphicsManager {
private:
    struct DrawItem {
        std::weak_ptr<Resource::SceneGeometryNode>   node;
        std::shared_ptr<MeshInfo>                    meshBuffer;
        std::weak_ptr<Resource::SceneObjectMaterial> material;
        std::string                                  psoName;
        ObjectConstants                              constants;
        size_t                                       constantBufferIndex;
        unsigned                                     numFramesDirty;
    };

    using FR = FrameResource<FrameConstants, ObjectConstants>;

public:
    int  Initialize() final;
    void Finalize() final;
    void Draw() final;
    void Clear() final;

    void RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) final;
#if defined(_DEBUG)
    void DrawDebugMesh(const Resource::SceneObjectMesh& mesh, const mat4f& transform, const vec4f& color) final;
    void ClearDebugBuffers() final;
#endif  // DEBUG

protected:
    void UpdateConstants() final;
    void InitializeBuffers(const Resource::Scene& scene) final;
    bool InitializeShaders() final;
    void ClearBuffers() final;
    void ClearShaders() final;
    void RenderBuffers() final;

private:
    int InitD3D();

    void PopulateCommandList(CommandContext& context);

    void CreateDescriptorHeaps();
    void CreateFrameResource();
    void CreateRootSignature();
    void CreateConstantBuffer();
    void CreateSampler();
    void BuildPipelineStateObject();

    static D3D_PRIMITIVE_TOPOLOGY GetD3DPrimitiveType(const Resource::PrimitiveType& primitiveType);

    void DrawRenderItems(CommandContext& context, const std::vector<DrawItem>& drawItems);

    static constexpr unsigned m_FrameCount     = 3;
    unsigned                  m_MaxObjects     = 1000;
    unsigned                  m_MaxTextures    = 10000;
    int                       m_CurrBackBuffer = 0;

    DescriptorAllocation m_CbvSrvUavDescriptors;
    DescriptorAllocation m_SamplerDescriptors;

    unsigned m_FrameCBOffset;
    unsigned m_SrvOffset;

    DXGI_FORMAT m_BackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D32_FLOAT;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_SwapChain;
    ColorBuffer                             m_DisplayPlanes[m_FrameCount];
    DepthBuffer                             m_SceneDepthBuffer;
    D3D12_VIEWPORT                          m_Viewport;
    D3D12_RECT                              m_ScissorRect;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

    std::unordered_map<std::string, GraphicsPSO> m_GraphicsPSO;
    std::unordered_map<std::string, ComputePSO>  m_ComputePSO;
    RootSignature                                m_RootSignature;

    std::vector<std::unique_ptr<FR>> m_FrameResource;
    // Generally, the frame resource size is greater or equal to frame count
    size_t m_FrameResourceSize      = m_FrameCount;
    size_t m_CurrFrameResourceIndex = 0;

    std::unordered_map<xg::Guid, TextureBuffer>             m_Textures;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshInfo>> m_Meshes;
    std::vector<DrawItem>                                   m_DrawItems;

#if defined(_DEBUG)
    std::vector<std::shared_ptr<Resource::SceneGeometryNode>> m_DebugNode;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                     m_DebugInputLayout;
    std::vector<DrawItem>                                     m_DebugDrawItems;
#endif  // DEBUG
};
}  // namespace Hitagi::Graphics::DX12
