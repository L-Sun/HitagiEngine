#pragma once
#include <vector>

#include "GraphicsManager.hpp"
#include "d3dUtil.hpp"

using namespace Microsoft::WRL;

namespace My {

class D3D12GraphicsManager : public GraphicsManager {
public:
    int  Initialize() final;
    void Finalize() final;
    void Draw() final;
    void Clear() final;

protected:
    bool SetPerFrameShaderParameters();
    bool SetPerBatchShaderParameters(int32_t index);

    void UpdateConstants() final;
    void InitializeBuffers(const Scene& scene) final;
    bool InitializeShaders() final;
    void ClearBuffers() final;
    void ClearShaders() final;
    void RenderBuffers() final;

private:
    int  InitD3D();
    void CreateCommandObjects();
    void CreateSwapChain();
    void FlushCommandQueue();
    void CreateDescriptorHeaps();
    void PopulateCommandList();

    void CreateVertexBuffer(const SceneObjectVertexArray& vertexArray);
    void CreateIndexBuffer(const SceneObjectIndexArray& indexArray);
    void CreateConstantBuffer();
    void CreateRootSignature();
    void BuildPipelineStateObject();

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    static constexpr int m_nSwapChainBufferCount = 2;
    int                  m_nCurrBackBuffer       = 0;

    ComPtr<IDXGIFactory7> m_pDxgiFactory;
    ComPtr<ID3D12Device5> m_pDevice;

    ComPtr<ID3D12CommandQueue>         m_pCommandQueue;
    ComPtr<ID3D12CommandAllocator>     m_pCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList4> m_pCommandList;
    ComPtr<ID3D12Fence1>               m_pFence;
    uint64_t                           m_nCurrenFence = 0;

    uint64_t                     m_nRtvHeapSize       = 0;
    uint64_t                     m_nDsvHeapSize       = 0;
    uint64_t                     m_nCbvSrvUavHeapSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_pCbvHeap;

    DXGI_FORMAT m_BackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ComPtr<IDXGISwapChain4> m_pSwapChain;
    ComPtr<ID3D12Resource>  m_pRenderTargets[m_nSwapChainBufferCount];
    ComPtr<ID3D12Resource>  m_pDepthStencilBuffer;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;

    bool     m_b4xMsaaState   = false;
    uint32_t m_n4xMsaaQuality = 0;

    D3D12_SHADER_BYTECODE    m_VS;
    D3D12_SHADER_BYTECODE    m_PS;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    ComPtr<ID3D12PipelineState> m_pPipelineState;
    ComPtr<ID3D12RootSignature> m_pRootSignature;

    struct ObjectConstants {
        mat4 modelMatrix;
    };

    struct D3D12DrawBatchContext : public DrawBatchContext {
        size_t property_count;
    };

    std::vector<D3D12DrawBatchContext>    m_DrawBatchContext;
    std::vector<D3D12_VERTEX_BUFFER_VIEW> m_vertexBufferView;
    std::vector<D3D12_INDEX_BUFFER_VIEW>  m_indexBufferView;

    std::unique_ptr<d3dUtil::UploadBuffer<ObjectConstants>>  m_pObjUploader;
    std::unique_ptr<d3dUtil::UploadBuffer<DrawFrameContext>> m_pFrameUploader;

    std::vector<ComPtr<ID3D12Resource>> m_Buffers;
};
}  // namespace My
