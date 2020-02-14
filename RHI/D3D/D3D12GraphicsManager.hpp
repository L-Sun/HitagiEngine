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

#if defined(DEBUG)
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

    static constexpr unsigned m_nFrameCount     = 3;
    static constexpr unsigned m_nMaxObjects     = 10000;
    static constexpr unsigned m_nMaxTextures    = 10000;
    int                       m_nCurrBackBuffer = 0;

    ComPtr<IDXGIFactory7> m_pDxgiFactory;
    ComPtr<ID3D12Device5> m_pDevice;

    ComPtr<ID3D12CommandQueue>         m_pCommandQueue;
    ComPtr<ID3D12CommandAllocator>     m_pCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList4> m_pCommandList;
    ComPtr<ID3D12Fence1>               m_pFence;
    HANDLE                             m_fenceEvent;
    uint64_t                           m_nCurrFence = 0;

    uint64_t                     m_nRtvHeapSize       = 0;
    uint64_t                     m_nDsvHeapSize       = 0;
    uint64_t                     m_nCbvSrvUavHeapSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_pCbvSrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_pSamplerHeap;
    unsigned                     m_nFrameCBOffset;
    unsigned                     m_nSrvOffset;

    DXGI_FORMAT m_BackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ComPtr<IDXGISwapChain4> m_pSwapChain;
    ComPtr<ID3D12Resource>  m_pRenderTargets[m_nFrameCount];
    ComPtr<ID3D12Resource>  m_pDepthStencilBuffer;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;

    bool     m_b4xMsaaState   = false;
    uint32_t m_n4xMsaaQuality = 0;

    std::unordered_map<std::string, D3D12_SHADER_BYTECODE> m_VS;
    std::unordered_map<std::string, D3D12_SHADER_BYTECODE> m_PS;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                  m_inputLayout;

    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>
        m_pipelineState;
    ComPtr<ID3D12RootSignature> m_pRootSignature;

    std::vector<std::unique_ptr<FR>> m_frameResource;
    // Generally, the frame resource size is greater or equal to frame count
    size_t m_nFrameResourceSize = m_nFrameCount;
    // size_t m_nFrameResourceSize      = 3;
    size_t m_nCurrFrameResourceIndex = 0;

    std::unordered_map<xg::Guid, std::shared_ptr<TextureBuffer>> m_textures;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshBuffer>>    m_meshBuffer;
    std::vector<DrawItem>                                        m_drawItems;

    std::vector<ComPtr<ID3D12Resource>> m_Uploader;

#if defined(DEBUG)
    std::vector<std::shared_ptr<SceneGeometryNode>>              m_debugNode;
    std::unordered_map<std::string, std::shared_ptr<MeshBuffer>> m_debugMeshBuffer;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                        m_debugInputLayout;
    std::vector<DrawItem>                                        m_debugDrawItems;
#endif  // DEBUG
};
}  // namespace My
