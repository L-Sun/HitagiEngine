#pragma once
#include <vector>

#include "GraphicsManager.hpp"
#include "d3dUtil.hpp"
#include "FrameResource.hpp"

using namespace Microsoft::WRL;

namespace My {

class D3D12GraphicsManager : public GraphicsManager {
private:
    struct ObjectConstants {
        mat4f modelMatrix;
        vec4f baseColor;
        vec4f specularColor;
        float specularPower;
    };

    struct TextureBuffer {
        size_t                 index;  // offset in current frame in srv
        ComPtr<ID3D12Resource> texture    = nullptr;
        ComPtr<ID3D12Resource> uploadHeap = nullptr;
    };

    struct MeshBuffer {
        std::vector<ComPtr<ID3D12Resource>>   verticesBuffer;
        std::vector<D3D12_VERTEX_BUFFER_VIEW> vbv;
        size_t                                indexCount;
        ComPtr<ID3D12Resource>                indicesBuffer;
        D3D12_INDEX_BUFFER_VIEW               ibv;
        D3D_PRIMITIVE_TOPOLOGY                primitiveType;
        std::weak_ptr<SceneObjectMaterial>    material;
        ComPtr<ID3D12PipelineState>           pPSO;
    };

    struct DrawItem {
        std::weak_ptr<SceneGeometryNode> node;
        std::shared_ptr<MeshBuffer>      meshBuffer;
        size_t                           constantBufferIndex;
        unsigned                         numFramesDirty;
    };

    using FR = FrameResource<FrameConstants, ObjectConstants>;

public:
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
    void InitializeBuffers(const Scene& scene) final;
    bool InitializeShaders() final;
    void ClearBuffers() final;
    void ClearShaders() final;
    void RenderBuffers() final;

private:
    int InitD3D();

    void CreateSwapChain();
    void CreateCommandObjects();
    void PopulateCommandList();
    void FlushCommandQueue();

    void                           CreateDescriptorHeaps();
    void                           CreateVertexBuffer(const SceneObjectVertexArray& vertexArray, const std::shared_ptr<MeshBuffer>& dbc);
    void                           CreateIndexBuffer(const SceneObjectIndexArray& indexArray, const std::shared_ptr<MeshBuffer>& dbc);
    void                           SetPrimitiveType(const PrimitiveType& primitiveType, const std::shared_ptr<MeshBuffer>& dbc);
    void                           CreateFrameResource();
    void                           CreateRootSignature();
    void                           CreateConstantBuffer();
    std::shared_ptr<TextureBuffer> CreateTextureBuffer(const Image& img, size_t srvOffset);
    void                           CreateSampler();
    void                           BuildPipelineStateObject();

    void DrawRenderItems(ID3D12GraphicsCommandList4* cmdList, const std::vector<DrawItem>& drawItems);

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    static constexpr unsigned m_FrameCount     = 3;
    static constexpr unsigned m_MaxObjects     = 10000;
    static constexpr unsigned m_MaxTextures    = 10000;
    int                       m_CurrBackBuffer = 0;

    ComPtr<IDXGIFactory7> m_DxgiFactory;
    ComPtr<ID3D12Device5> m_Device;

    ComPtr<ID3D12CommandQueue>         m_CommandQueue;
    ComPtr<ID3D12CommandAllocator>     m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList4> m_CommandList;
    ComPtr<ID3D12Fence1>               m_Fence;
    HANDLE                             m_FenceEvent;
    uint64_t                           m_CurrFence = 0;

    uint64_t                     m_RtvHeapSize       = 0;
    uint64_t                     m_DsvHeapSize       = 0;
    uint64_t                     m_CbvSrvUavHeapSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_CbvSrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_SamplerHeap;
    unsigned                     m_FrameCBOffset;
    unsigned                     m_SrvOffset;

    DXGI_FORMAT m_BackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ComPtr<IDXGISwapChain4> m_SwapChain;
    ComPtr<ID3D12Resource>  m_RenderTargets[m_FrameCount];
    ComPtr<ID3D12Resource>  m_DepthStencilBuffer;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;

    bool     m_4xMsaaState   = false;
    uint32_t m_4xMsaaQuality = 0;

    std::unordered_map<std::string, D3D12_SHADER_BYTECODE> m_VS;
    std::unordered_map<std::string, D3D12_SHADER_BYTECODE> m_PS;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                  m_InputLayout;

    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>
        m_PipelineState;
    ComPtr<ID3D12RootSignature> m_RootSignature;

    std::vector<std::unique_ptr<FR>> m_FrameResource;
    // Generally, the frame resource size is greater or equal to frame count
    size_t m_FrameResourceSize = m_FrameCount;
    // size_t m_FrameResourceSize      = 3;
    size_t m_CurrFrameResourceIndex = 0;

    std::unordered_map<xg::Guid, std::shared_ptr<TextureBuffer>> m_Textures;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshBuffer>>    m_MeshBuffer;
    std::vector<DrawItem>                                        m_DrawItems;

    std::vector<ComPtr<ID3D12Resource>> m_Uploader;

#if defined(_DEBUG)
    std::vector<std::shared_ptr<SceneGeometryNode>>              m_DebugNode;
    std::unordered_map<std::string, std::shared_ptr<MeshBuffer>> m_DebugMeshBuffer;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                        m_DebugInputLayout;
    std::vector<DrawItem>                                        m_DebugDrawItems;
#endif  // DEBUG
};
}  // namespace My
