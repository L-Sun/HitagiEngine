#pragma once
#include "GraphicsManager.hpp"
#include "d3dUtil.hpp"
#include "FrameResource.hpp"
#include "CommandListManager.hpp"
#include "CommandContext.hpp"
#include "GpuBuffer.hpp"
#include "RootSignature.hpp"
#include "DescriptorAllocator.hpp"
#include "PipeLineState.hpp"

namespace Hitagi::Graphics::DX12 {

class D3D12GraphicsManager : public GraphicsManager {
    friend class LinearAllocator;
    friend class DescriptorAllocatorPage;
    friend class DynamicDescriptorHeap;
    friend class RootSignature;
    friend class DescriptorAllocation;
    friend class GraphicsPSO;

private:
    struct ObjectConstants {
        mat4f modelMatrix;
        vec4f baseColor;
        vec4f specularColor;
        float specularPower;
    };

    struct MeshInfo {
        size_t                                       indexCount;
        std::vector<D3D12_VERTEX_BUFFER_VIEW>        vbv;
        D3D12_INDEX_BUFFER_VIEW                      ibv;
        D3D_PRIMITIVE_TOPOLOGY                       primitiveType;
        std::weak_ptr<Resource::SceneObjectMaterial> material;
        ID3D12PipelineState*                         pPSO;
    };

    struct DrawItem {
        std::weak_ptr<Resource::SceneGeometryNode> node;
        std::shared_ptr<MeshInfo>                  meshBuffer;
        size_t                                     constantBufferIndex;
        unsigned                                   numFramesDirty;
    };

    using FR = FrameResource<FrameConstants, ObjectConstants>;

public:
    static D3D12GraphicsManager& Get() {
        static D3D12GraphicsManager instance;
        return instance;
    }

    D3D12GraphicsManager(const D3D12GraphicsManager&) = delete;
    D3D12GraphicsManager(D3D12GraphicsManager&&)      = delete;
    D3D12GraphicsManager& operator=(const D3D12GraphicsManager&) = delete;
    D3D12GraphicsManager& operator=(D3D12GraphicsManager&&) = delete;

    int  Initialize() final;
    void Finalize() final;
    void Draw() final;
    void Clear() final;

#if defined(_DEBUG)
    void RenderLine(const vec3f& from, const vec3f& to, const vec3f& color) final;
    void RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) final;
    void RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) final;
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
    D3D12GraphicsManager() {}
    virtual ~D3D12GraphicsManager() {}

    int InitD3D();

    void CreateSwapChain();
    void PopulateCommandList(CommandContext& context);

    void CreateDescriptorHeaps();
    void CreateVertexBuffer(const Resource::SceneObjectVertexArray& vertexArray, const std::shared_ptr<MeshInfo>& dbc);
    void CreateIndexBuffer(const Resource::SceneObjectIndexArray& indexArray, const std::shared_ptr<MeshInfo>& dbc);
    void SetPrimitiveType(const Resource::PrimitiveType& primitiveType, const std::shared_ptr<MeshInfo>& dbc);
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
    static constexpr unsigned m_MaxObjects     = 10000;
    static constexpr unsigned m_MaxTextures    = 10000;
    int                       m_CurrBackBuffer = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_DxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device6> m_Device;

    CommandListManager m_CommandListManager;

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
    std::vector<GpuBuffer>                                  m_Buffers;

#if defined(_DEBUG)
    std::vector<std::shared_ptr<Resource::SceneGeometryNode>>  m_DebugNode;
    std::unordered_map<std::string, std::shared_ptr<MeshInfo>> m_DebugMeshBuffer;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                      m_DebugInputLayout;
    std::vector<DrawItem>                                      m_DebugDrawItems;
#endif  // DEBUG
};
}  // namespace Hitagi::Graphics::DX12
