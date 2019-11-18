#pragma once
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "GraphicsManager.hpp"

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
    int                         InitD3d();
    void                        CreateCommandObjects();
    void                        CreateSwapChain();
    void                        FlushCommandQueue();
    void                        CreateRtvAndDsvDescHeaps();
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    static constexpr int m_nSwapChainBufferCount = 2;
    int                  m_nCurrBackBuffer       = 0;

    ComPtr<ID3D12RootSignature> m_pRootSignature = nullptr;
    ComPtr<IDXGIFactory7>       m_pDxgiFactory   = nullptr;
    ComPtr<ID3D12Device5>       m_pDevice        = nullptr;
    ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr;

    ComPtr<ID3D12CommandQueue>         m_pCommandQueue     = nullptr;
    ComPtr<ID3D12CommandAllocator>     m_pCommandAllocator = nullptr;
    ComPtr<ID3D12GraphicsCommandList4> m_pCommandList      = nullptr;
    ComPtr<ID3D12Fence1>               m_pFence            = nullptr;
    uint64_t                           m_nCurrenFence      = 0;

    uint64_t                     m_nRtvHeapSize       = 0;
    uint64_t                     m_nDsvHeapSize       = 0;
    uint64_t                     m_nCbvSrvUavHeapSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap           = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap           = nullptr;

    DXGI_FORMAT m_BackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    ComPtr<IDXGISwapChain4> m_pSwapChain = nullptr;
    ComPtr<ID3D12Resource>  m_pRenderTargets[m_nSwapChainBufferCount];
    ComPtr<ID3D12Resource>  m_pDepthStencilBuffer = nullptr;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT     m_scissorRect;

    bool     m_b4xMsaaState   = false;
    uint32_t m_n4xMsaaQuality = 0;
};
}  // namespace My
