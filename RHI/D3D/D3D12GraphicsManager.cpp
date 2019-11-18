#include <iostream>
#include "D3d12GraphicsManager.hpp"
#include "WindowsApplication.hpp"
#include "d3dUtil.hpp"

using namespace My;
using namespace std;

int D3d12GraphicsManager::Initialize() {
    int result = GraphicsManager::Initialize();
    result     = InitD3d();
    return result;
}

void D3d12GraphicsManager::Finalize() {
    if (m_pDevice) FlushCommandQueue();
    GraphicsManager::Finalize();
}

void D3d12GraphicsManager::Draw() { GraphicsManager::Draw(); }
void D3d12GraphicsManager::Clear() { GraphicsManager::Clear(); }

// this is the function that loads and prepares the shaders
bool D3d12GraphicsManager::InitializeShaders() { return true; }

void D3d12GraphicsManager::ClearShaders() {}

void D3d12GraphicsManager::InitializeBuffers(const Scene& scene) {}

void D3d12GraphicsManager::ClearBuffers() {}

void D3d12GraphicsManager::UpdateConstants() {
    GraphicsManager::UpdateConstants();
}

void D3d12GraphicsManager::RenderBuffers() {}

bool D3d12GraphicsManager::SetPerFrameShaderParameters() { return true; }

bool D3d12GraphicsManager::SetPerBatchShaderParameters(int32_t index) {
    return true;
}

int D3d12GraphicsManager::InitD3d() {
    auto config =
        reinterpret_cast<WindowsApplication*>(g_pApp)->GetConfiguration();

    unsigned dxgiFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)

    // Enable d3d12 debug layer.
    {
        ComPtr<ID3D12Debug3> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif  // DEBUG
    ThrowIfFailed(
        CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_pDxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                     IID_PPV_ARGS(&m_pDevice)))) {
            ComPtr<IDXGIAdapter4> pWarpaAdapter;
            ThrowIfFailed(
                m_pDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpaAdapter)));
            ThrowIfFailed(D3D12CreateDevice(pWarpaAdapter.Get(),
                                            D3D_FEATURE_LEVEL_11_0,
                                            IID_PPV_ARGS(&m_pDevice)));
        }
    }

    // Create fence and get size of descriptor
    {
        ThrowIfFailed(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                             IID_PPV_ARGS(&m_pFence)));
        m_nRtvHeapSize = m_pDevice->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_nDsvHeapSize = m_pDevice->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_nCbvSrvUavHeapSize = m_pDevice->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Detect 4x MSAA
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Flags  = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.Format = m_BackBufferFormat;
        msQualityLevels.NumQualityLevels = 0;
        msQualityLevels.SampleCount      = 4;
        ThrowIfFailed(m_pDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels,
            sizeof(msQualityLevels)));

        m_n4xMsaaQuality = msQualityLevels.NumQualityLevels;
        assert(m_n4xMsaaQuality > 0 && "Unexpected MSAA qulity level.");
    }

    // Create command queue, allocator and list.
    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescHeaps();

    // Create rtv.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
            m_pRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (unsigned i = 0; i < m_nSwapChainBufferCount; i++) {
            ThrowIfFailed(
                m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i])));
            m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(),
                                              nullptr, rtvHandle);
            rtvHandle.Offset(1, m_nRtvHeapSize);
        }
    }

    // Create dsv.
    {
        D3D12_RESOURCE_DESC dsvDesc = {};
        dsvDesc.Alignment           = 0;
        dsvDesc.DepthOrArraySize    = 1;
        dsvDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dsvDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        dsvDesc.Format              = m_DepthStencilFormat;
        dsvDesc.Height              = config.screenHeight;
        dsvDesc.Width               = config.screenWidth;
        dsvDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        dsvDesc.MipLevels           = 1;
        dsvDesc.SampleDesc.Count    = m_b4xMsaaState ? 4 : 1;
        dsvDesc.SampleDesc.Quality =
            m_b4xMsaaState ? (m_n4xMsaaQuality - 1) : 0;

        D3D12_CLEAR_VALUE optClear    = {};
        optClear.Format               = m_DepthStencilFormat;
        optClear.DepthStencil.Depth   = 1.0f;
        optClear.DepthStencil.Stencil = 0;

        auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_pDevice->CreateCommittedResource(
            &heap_properties, D3D12_HEAP_FLAG_NONE, &dsvDesc,
            D3D12_RESOURCE_STATE_COMMON, &optClear,
            IID_PPV_ARGS(&m_pDepthStencilBuffer)));

        m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr,
                                          DepthStencilView());

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_pCommandList->Reset(m_pCommandAllocator.Get(),
                              m_pPipelineState.Get());
        m_pCommandList->ResourceBarrier(1, &barrier);
    }

    // Set viewport and scissor rect.
    {
        m_viewport.Width    = static_cast<float>(config.screenWidth);
        m_viewport.Height   = static_cast<float>(config.screenHeight);
        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;

        m_scissorRect.top    = 0.0f;
        m_scissorRect.left   = 0.0f;
        m_scissorRect.bottom = static_cast<float>(config.screenHeight);
        m_scissorRect.right  = static_cast<float>(config.screenWidth);

        m_pCommandList->RSSetViewports(1, &m_viewport);
        m_pCommandList->RSSetScissorRects(1, &m_scissorRect);
    }

    return 0;
}

void D3d12GraphicsManager::CreateSwapChain() {
    m_pSwapChain.Reset();
    ComPtr<IDXGISwapChain1> swapChain;
    DXGI_SWAP_CHAIN_DESC1   swapChainDesc = {};
    swapChainDesc.BufferCount             = m_nSwapChainBufferCount;
    swapChainDesc.Width            = g_pApp->GetConfiguration().screenWidth;
    swapChainDesc.Height           = g_pApp->GetConfiguration().screenHeight;
    swapChainDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Scaling          = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode        = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = m_b4xMsaaState ? 4 : 1;
    swapChainDesc.SampleDesc.Quality =
        m_b4xMsaaState ? (m_n4xMsaaQuality - 1) : 0;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    HWND window =
        reinterpret_cast<WindowsApplication*>(g_pApp)->GetMainWindow();
    ThrowIfFailed(m_pDxgiFactory->CreateSwapChainForHwnd(
        m_pCommandQueue.Get(), window, &swapChainDesc, nullptr, nullptr,
        &swapChain));

    ThrowIfFailed(swapChain.As(&m_pSwapChain));
}

void D3d12GraphicsManager::CreateCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    // Create command queue
    ThrowIfFailed(m_pDevice->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&m_pCommandQueue)));
    // Create command allocator
    ThrowIfFailed(m_pDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator)));
    // Crete command lists
    ThrowIfFailed(m_pDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(),
        m_pPipelineState.Get(), IID_PPV_ARGS(&m_pCommandList)));
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_pCommandList->Close());
}

void D3d12GraphicsManager::FlushCommandQueue() {
    m_nCurrenFence++;
    ThrowIfFailed(m_pCommandQueue->Signal(m_pFence.Get(), m_nCurrenFence));
    if (m_pFence->GetCompletedValue() < m_nCurrenFence) {
        HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);
        ThrowIfFailed(
            m_pFence->SetEventOnCompletion(m_nCurrenFence, eventHandle));

        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void D3d12GraphicsManager::CreateRtvAndDsvDescHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask                   = 0;
    rtvHeapDesc.NumDescriptors             = m_nSwapChainBufferCount;
    rtvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&rtvHeapDesc,
                                                  IID_PPV_ARGS(&m_pRtvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask                   = 0;
    dsvHeapDesc.NumDescriptors             = 1;
    dsvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&dsvHeapDesc,
                                                  IID_PPV_ARGS(&m_pDsvHeap)));
}

D3D12_CPU_DESCRIPTOR_HANDLE D3d12GraphicsManager::CurrentBackBufferView()
    const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pRtvHeap->GetCPUDescriptorHandleForHeapStart(), m_nCurrBackBuffer,
        m_nRtvHeapSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3d12GraphicsManager::DepthStencilView() const {
    return m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
}