#include <iostream>
#include <objbase.h>
#include "D3D12GraphicsManager.hpp"
#include "WindowsApplication.hpp"

using namespace My;
using namespace std;
int D3D12GraphicsManager::Initialize() {
    int result = GraphicsManager::Initialize();
    result     = InitD3D();
    return result;
}

void D3D12GraphicsManager::Finalize() {
    FlushCommandQueue();
    GraphicsManager::Finalize();
}

void D3D12GraphicsManager::Draw() { GraphicsManager::Draw(); }
void D3D12GraphicsManager::Clear() { GraphicsManager::Clear(); }

// this is the function that loads and prepares the shaders
bool D3D12GraphicsManager::InitializeShaders() {
    Buffer v_shader =
               g_pAssetLoader->SyncOpenAndReadBinary("Shaders/simple_vs.cso"),
           p_shader =
               g_pAssetLoader->SyncOpenAndReadBinary("Shaders/simple_ps.cso");

    // Define the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout    = {inputElementDescs, _countof(inputElementDescs)};
    psoDesc.pRootSignature = m_pRootSignature.Get();
    psoDesc.VS =
        CD3DX12_SHADER_BYTECODE(v_shader.GetData(), v_shader.GetDataSize());
    ;
    psoDesc.PS =
        CD3DX12_SHADER_BYTECODE(p_shader.GetData(), p_shader.GetDataSize());
    ;
    psoDesc.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = m_BackBufferFormat;
    psoDesc.SampleDesc.Count      = m_b4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality    = m_b4xMsaaState ? (m_n4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat             = m_DepthStencilFormat;

    ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_pPipelineState)));

    return true;
}

void D3D12GraphicsManager::ClearShaders() {}

void D3D12GraphicsManager::InitializeBuffers(const Scene& scene) {
    ThrowIfFailed(m_pCommandList->Reset(m_pCommandAllocator.Get(),
                                        m_pPipelineState.Get()));
    m_pVertexBufferGPU = d3dUtil::CreateDefaultBuffer(
        m_pDevice.Get(), m_pCommandList.Get(), m_triangle, sizeof(m_triangle),
        m_pUploadBuffer);

    m_vertexBufferView.BufferLocation =
        m_pVertexBufferGPU->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes   = sizeof(m_triangle);
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);

    ThrowIfFailed(m_pCommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
}

void D3D12GraphicsManager::ClearBuffers() {}

void D3D12GraphicsManager::UpdateConstants() {
    GraphicsManager::UpdateConstants();

    // Upader buffer
}

void D3D12GraphicsManager::RenderBuffers() {
    PopulateCommandList();
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(m_pSwapChain->Present(0, 0));
    m_nCurrBackBuffer = (m_nCurrBackBuffer + 1) % m_nSwapChainBufferCount;

    FlushCommandQueue();
}

bool D3D12GraphicsManager::SetPerFrameShaderParameters() { return true; }

bool D3D12GraphicsManager::SetPerBatchShaderParameters(int32_t index) {
    return true;
}

int D3D12GraphicsManager::InitD3D() {
    auto config = g_pApp->GetConfiguration();

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

    ThrowIfFailed(m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr));

    CreateSwapChain();
    CreateRtvAndDsv();
    CreateRootSignature();

    ThrowIfFailed(m_pCommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

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
    }

    return 0;
}

void D3D12GraphicsManager::CreateSwapChain() {
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
        reinterpret_cast<WindowsApplication*>(g_pApp.get())->GetMainWindow();
    ThrowIfFailed(m_pDxgiFactory->CreateSwapChainForHwnd(
        m_pCommandQueue.Get(), window, &swapChainDesc, nullptr, nullptr,
        &swapChain));

    ThrowIfFailed(swapChain.As(&m_pSwapChain));
}

void D3D12GraphicsManager::CreateRootSignature() {
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        0, nullptr, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_pDevice->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_pRootSignature)));
}

void D3D12GraphicsManager::CreateCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    // Create command queue
    ThrowIfFailed(m_pDevice->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&m_pCommandQueue)));
    // Create command allocator
    ThrowIfFailed(m_pDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(m_pCommandAllocator.GetAddressOf())));
    // Crete command lists
    ThrowIfFailed(m_pDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(),
        m_pPipelineState.Get(), IID_PPV_ARGS(&m_pCommandList)));
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_pCommandList->Close());
}

void D3D12GraphicsManager::FlushCommandQueue() {
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

void D3D12GraphicsManager::CreateRtvAndDsv() {
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

    // Create rtv.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_pRtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (unsigned i = 0; i < m_nSwapChainBufferCount; i++) {
        ThrowIfFailed(
            m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i])));
        m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr,
                                          rtvHandle);
        rtvHandle.Offset(1, m_nRtvHeapSize);
    }

    // Create dsv.
    auto config = g_pApp->GetConfiguration();

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
    dsvDesc.SampleDesc.Quality  = m_b4xMsaaState ? (m_n4xMsaaQuality - 1) : 0;

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

    m_pCommandList->ResourceBarrier(1, &barrier);
}

void D3D12GraphicsManager::PopulateCommandList() {
    ThrowIfFailed(m_pCommandAllocator->Reset());

    ThrowIfFailed(m_pCommandList->Reset(m_pCommandAllocator.Get(),
                                        m_pPipelineState.Get()));

    m_pCommandList->RSSetViewports(1, &m_viewport);
    m_pCommandList->RSSetScissorRects(1, &m_scissorRect);

    // change state from presentation to waitting to render.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pRenderTargets[m_nCurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_pCommandList->ResourceBarrier(1, &barrier);

    // clear buffer view
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_pCommandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor,
                                          0, nullptr);
    m_pCommandList->ClearDepthStencilView(
        DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    // render back buffer
    const D3D12_CPU_DESCRIPTOR_HANDLE currBackBuffer = CurrentBackBufferView();
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv            = DepthStencilView();
    m_pCommandList->OMSetRenderTargets(1, &currBackBuffer, false, &dsv);

    m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());

    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_pCommandList->DrawInstanced(3, 1, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pRenderTargets[m_nCurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_pCommandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_pCommandList->Close());
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::CurrentBackBufferView()
    const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pRtvHeap->GetCPUDescriptorHandleForHeapStart(), m_nCurrBackBuffer,
        m_nRtvHeapSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::DepthStencilView() const {
    return m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
