#pragma once
#include "GraphicsManager.hpp"
#include "FrameResource.hpp"
#include "CommandListManager.hpp"
#include "CommandContext.hpp"
#include "GpuBuffer.hpp"
#include "RootSignature.hpp"
#include "DescriptorAllocator.hpp"
#include "PipeLineState.hpp"

namespace Hitagi::Graphics::DX12 {

class D3D12GraphicsManager : public GraphicsManager {
private:
    struct ObjectConstants {
        mat4f modelMatrix;
        vec4f ambientColor;
        vec4f diffuseColor;
        vec4f specularColor;
        float specularPower;
    };

    struct MeshInfo {
        size_t                 indexCount;
        std::vector<GpuBuffer> verticesBuffer;
        GpuBuffer              indicesBuffer;
        D3D_PRIMITIVE_TOPOLOGY primitiveType;
    };

    struct DrawItem {
        std::weak_ptr<Resource::SceneGeometryNode>   node;
        std::shared_ptr<MeshInfo>                    meshBuffer;
        std::weak_ptr<Resource::SceneObjectMaterial> material;
        std::string                                  psoName;
        size_t                                       constantBufferIndex;
        unsigned                                     numFramesDirty;
    };

    using FR = FrameResource<FrameConstants, ObjectConstants>;

public:
    int  Initialize() final;
    void Finalize() final;
    void Draw() final;
    void Clear() final;

    void ToggleRayTrancing() { m_Raster = !m_Raster; }

    void RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) final;
#if defined(_DEBUG)
    void RenderLine(const vec3f& from, const vec3f& to, const vec3f& color) final;
    void RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) final;
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
    void CreateVertexBuffer(const Resource::SceneObjectVertexArray& vertexArray, std::shared_ptr<MeshInfo> dbc);
    void CreateIndexBuffer(const Resource::SceneObjectIndexArray& indexArray, std::shared_ptr<MeshInfo> dbc);
    void SetPrimitiveType(const Resource::PrimitiveType& primitiveType, std::shared_ptr<MeshInfo> dbc);
    void CreateFrameResource();
    void CreateRootSignature();
    void CreateConstantBuffer();
    TextureBuffer CreateTextureBuffer(const Resource::Image& image, size_t srvOffset);
    void          CreateSampler();
    void          BuildPipelineStateObject();

    void DrawRenderItems(CommandContext& context, const std::vector<DrawItem>& drawItems);

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    static constexpr unsigned m_FrameCount     = 3;
    unsigned                  m_MaxObjects     = 1000;
    unsigned                  m_MaxTextures    = 10000;
    int                       m_CurrBackBuffer = 0;
    bool                      m_Raster         = false;

    uint64_t m_RtvHeapSize       = 0;
    uint64_t m_DsvHeapSize       = 0;
    uint64_t m_CbvSrvUavHeapSize = 0;

    DescriptorAllocator  m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    DescriptorAllocation m_RtvDescriptors;
    DescriptorAllocation m_DsvDescriptors;
    DescriptorAllocation m_CbvSrvDescriptors;
    DescriptorAllocation m_SamplerDescriptors;

    unsigned m_FrameCBOffset;
    unsigned m_SrvOffset;

    DXGI_FORMAT m_BackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_SwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_RenderTargets[m_FrameCount];
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_DepthStencilBuffer;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;

    bool     m_4xMsaaState   = false;
    uint32_t m_4xMsaaQuality = 0;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

    std::unordered_map<std::string, GraphicsPSO> m_GraphicsPSO;
    RootSignature                                m_RootSignature;

    std::vector<std::unique_ptr<FR>> m_FrameResource;
    // Generally, the frame resource size is greater or equal to frame count
    size_t m_FrameResourceSize = m_FrameCount;
    // size_t m_FrameResourceSize      = 3;
    size_t m_CurrFrameResourceIndex = 0;

    std::unordered_map<xg::Guid, TextureBuffer>             m_Textures;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshInfo>> m_Meshes;
    std::vector<DrawItem>                                   m_DrawItems;

#if defined(_DEBUG)
    std::vector<std::shared_ptr<Resource::SceneGeometryNode>>  m_DebugNode;
    std::unordered_map<std::string, std::shared_ptr<MeshInfo>> m_DebugMeshBuffer;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                      m_DebugInputLayout;
    std::vector<DrawItem>                                      m_DebugDrawItems;
#endif  // DEBUG
};
}  // namespace Hitagi::Graphics::DX12
