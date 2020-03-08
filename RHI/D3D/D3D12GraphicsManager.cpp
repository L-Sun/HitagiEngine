#include <iostream>
#include <objbase.h>
#include "D3D12GraphicsManager.hpp"
#include "WindowsApplication.hpp"

namespace Hitagi {

int D3D12GraphicsManager::Initialize() {
    int result = GraphicsManager::Initialize();
    result     = InitD3D();
    return result;
}

void D3D12GraphicsManager::Finalize() { GraphicsManager::Finalize(); }

void D3D12GraphicsManager::Draw() {
    m_CurrFrameResourceIndex =
        (m_CurrFrameResourceIndex + 1) % m_FrameResourceSize;
    GraphicsManager::Draw();
}

void D3D12GraphicsManager::Clear() { GraphicsManager::Clear(); }

int D3D12GraphicsManager::InitD3D() {
    auto config = g_App->GetConfiguration();

    unsigned dxgiFactoryFlags = 0;

#if defined(_DEBUG)

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
        CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_DxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                     IID_PPV_ARGS(&m_Device)))) {
            ComPtr<IDXGIAdapter4> pWarpaAdapter;
            ThrowIfFailed(
                m_DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpaAdapter)));
            ThrowIfFailed(D3D12CreateDevice(pWarpaAdapter.Get(),
                                            D3D_FEATURE_LEVEL_11_0,
                                            IID_PPV_ARGS(&m_Device)));
        }
    }

    // Create fence and get size of descriptor
    {
        ThrowIfFailed(m_Device->CreateFence(
            m_CurrFence, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
        m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_FenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
        m_RtvHeapSize = m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_DsvHeapSize = m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_CbvSrvUavHeapSize = m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Detect 4x MSAA
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Flags            = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.Format           = m_BackBufferFormat;
        msQualityLevels.NumQualityLevels = 0;
        msQualityLevels.SampleCount      = 4;
        ThrowIfFailed(m_Device->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels,
            sizeof(msQualityLevels)));

        m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
        assert(m_4xMsaaQuality > 0 && "Unexpected MSAA qulity level.");
    }

    // Create command queue, allocator and list.
    CreateCommandObjects();
    CreateSwapChain();

    // Set viewport and scissor rect.
    {
        m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, config.screenWidth,
                                      config.screenHeight);
        m_ScissorRect =
            CD3DX12_RECT(0, 0, config.screenWidth, config.screenHeight);
    }

    CreateDescriptorHeaps();
    CreateFrameResource();
    CreateRootSignature();
    CreateConstantBuffer();
    CreateSampler();

    return 0;
}

void D3D12GraphicsManager::CreateSwapChain() {
    m_SwapChain.Reset();
    ComPtr<IDXGISwapChain1> swapChain;
    DXGI_SWAP_CHAIN_DESC1   swapChainDesc = {};
    swapChainDesc.BufferCount             = m_FrameCount;
    swapChainDesc.Width                   = g_App->GetConfiguration().screenWidth;
    swapChainDesc.Height                  = g_App->GetConfiguration().screenHeight;
    swapChainDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage             = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Scaling                 = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect              = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode               = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count        = m_4xMsaaState ? 4 : 1;
    swapChainDesc.SampleDesc.Quality =
        m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    HWND window =
        reinterpret_cast<WindowsApplication*>(g_App.get())->GetMainWindow();
    ThrowIfFailed(m_DxgiFactory->CreateSwapChainForHwnd(
        m_CommandQueue.Get(), window, &swapChainDesc, nullptr, nullptr,
        &swapChain));

    ThrowIfFailed(swapChain.As(&m_SwapChain));
    ThrowIfFailed(m_DxgiFactory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
}

void D3D12GraphicsManager::CreateCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    // Create command queue
    ThrowIfFailed(m_Device->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&m_CommandQueue)));
    // Create command allocator
    ThrowIfFailed(m_Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(m_CommandAllocator.GetAddressOf())));
    // Crete command lists
    ThrowIfFailed(m_Device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&m_CommandList)));
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_CommandList->Close());
}

void D3D12GraphicsManager::CreateDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask                   = 0;
    rtvHeapDesc.NumDescriptors             = m_FrameCount;
    rtvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&rtvHeapDesc,
                                                 IID_PPV_ARGS(&m_RtvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask                   = 0;
    dsvHeapDesc.NumDescriptors             = 1;
    dsvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&dsvHeapDesc,
                                                 IID_PPV_ARGS(&m_DsvHeap)));

    // Create rtv.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (unsigned i = 0; i < m_FrameCount; i++) {
        ThrowIfFailed(
            m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
        m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr,
                                         rtvHandle);
        rtvHandle.Offset(1, m_RtvHeapSize);
    }

    // Create dsv.
    auto config = g_App->GetConfiguration();

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
    dsvDesc.SampleDesc.Count    = m_4xMsaaState ? 4 : 1;
    dsvDesc.SampleDesc.Quality  = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;

    D3D12_CLEAR_VALUE optClear    = {};
    optClear.Format               = m_DepthStencilFormat;
    optClear.DepthStencil.Depth   = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heap_properties, D3D12_HEAP_FLAG_NONE, &dsvDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear,
        IID_PPV_ARGS(&m_DepthStencilBuffer)));

    m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr,
                                     DepthStencilView());

    // Create CBV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC CbvHeapDesc = {};
    CbvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CbvHeapDesc.NodeMask                   = 0;
    // per frame have n objects constant buffer descriptor and 1 frame constant
    // buffer descriptor
    CbvHeapDesc.NumDescriptors =
        m_FrameResourceSize * (m_MaxObjects + 1) + m_MaxTextures;
    CbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(
        &CbvHeapDesc, IID_PPV_ARGS(&m_CbvSrvHeap)));

    // Use the last n descriptor as the frame constant buffer descriptor
    m_FrameCBOffset = m_FrameResourceSize * m_MaxObjects;
    m_SrvOffset     = m_FrameResourceSize * (m_MaxObjects + 1);

    // Sampler descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC samplerDescHeap = {};
    samplerDescHeap.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    samplerDescHeap.NumDescriptors             = 1;
    samplerDescHeap.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(
        &samplerDescHeap, IID_PPV_ARGS(&m_SamplerHeap)));
}

void D3D12GraphicsManager::CreateFrameResource() {
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        m_FrameResource.push_back(
            std::make_unique<FR>(m_Device.Get(), m_MaxObjects));
    }
}

void D3D12GraphicsManager::CreateRootSignature() {
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(m_Device->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[4];
    // for constant per frame
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    // for constant per object
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    // for srv
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    // for sampler
    ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[4];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
    rootParameters[2].InitAsDescriptorTable(1, &ranges[2],
                                            D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsDescriptorTable(1, &ranges[3],
                                            D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(
        _countof(rootParameters), rootParameters, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
        &rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(m_Device->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_RootSignature)));
}

void D3D12GraphicsManager::CreateConstantBuffer() {
    // -----------------------------------------------------------
    // |0|1|2|...|n-1|     |n|n+1|...|2n-1|...... |mn-1|
    // 1st frame resrouce  2nd frame resource     end of nst frame
    // for object          for object             for object
    // |mn|mn+1|...|mn+m-1|
    // for m frame consant buffer
    // -----------------------------------------------------------
    auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_CbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
    // Create object constant buffer
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        auto                            fr = m_FrameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDsec;
        cbDsec.SizeInBytes = fr->m_ObjCBUploader->GetCBByteSize();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress =
            fr->m_ObjCBUploader->Resource()->GetGPUVirtualAddress();

        for (size_t j = 0; j < fr->m_ObjCBUploader->GetElementCount(); j++) {
            cbDsec.BufferLocation = cbAddress;
            m_Device->CreateConstantBufferView(&cbDsec, handle);
            cbAddress += cbDsec.SizeInBytes;
            handle.Offset(m_CbvSrvUavHeapSize);
        }
    }
    // Create frame constant buffer, handle have been at the begin of
    // last m descriptor area
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        auto                            fr = m_FrameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc;
        cbDesc.SizeInBytes = fr->m_FrameCBUploader->GetCBByteSize();
        cbDesc.BufferLocation =
            fr->m_FrameCBUploader->Resource()->GetGPUVirtualAddress();
        m_Device->CreateConstantBufferView(&cbDesc, handle);
        handle.Offset(m_CbvSrvUavHeapSize);
    }
}

void D3D12GraphicsManager::CreateSampler() {
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.ComparisonFunc     = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.Filter             = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    samplerDesc.MaxAnisotropy      = 1;
    samplerDesc.MaxLOD             = D3D12_FLOAT32_MAX;
    samplerDesc.MinLOD             = 0;
    samplerDesc.MipLODBias         = 0.0f;

    m_Device->CreateSampler(
        &samplerDesc, m_SamplerHeap->GetCPUDescriptorHandleForHeapStart());
}

bool D3D12GraphicsManager::InitializeShaders() {
    Buffer v_shader =
               g_AssetLoader->SyncOpenAndReadBinary("Asset/Shaders/simple_vs.cso"),
           p1_shader =
               g_AssetLoader->SyncOpenAndReadBinary("Asset/Shaders/simple_ps_1.cso");

    m_VS["simple"] =
        CD3DX12_SHADER_BYTECODE(v_shader.GetData(), v_shader.GetDataSize());
    m_PS["no_texture"] =
        CD3DX12_SHADER_BYTECODE(p1_shader.GetData(), p1_shader.GetDataSize());

    Buffer p2_shader =
        g_AssetLoader->SyncOpenAndReadBinary("Asset/Shaders/simple_ps_2.cso");
    m_PS["texture"] =
        CD3DX12_SHADER_BYTECODE(p2_shader.GetData(), p2_shader.GetDataSize());

    // Define the vertex input layout, becase vertices storage is
    m_InputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

#if defined(_DEBUG)
    Buffer v_debugShader =
               g_AssetLoader->SyncOpenAndReadBinary("Asset/Shaders/debug_vs.cso"),
           p_debugShader =
               g_AssetLoader->SyncOpenAndReadBinary("Asset/Shaders/debug_s.cso");
    m_VS["debug"] = CD3DX12_SHADER_BYTECODE(v_debugShader.GetData(),
                                            v_debugShader.GetDataSize());
    m_PS["debug"] = CD3DX12_SHADER_BYTECODE(p_debugShader.GetData(),
                                            p_debugShader.GetDataSize());

    m_DebugInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0},
                          {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0}};

#endif  // DEBUG

    BuildPipelineStateObject();

    return true;
}

void D3D12GraphicsManager::BuildPipelineStateObject() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc    = {};
    psoDesc.InputLayout                           = {m_InputLayout.data(),
                           static_cast<UINT>(m_InputLayout.size())};
    psoDesc.pRootSignature                        = m_RootSignature.Get();
    psoDesc.VS                                    = m_VS["simple"];
    psoDesc.PS                                    = m_PS["no_texture"];
    psoDesc.RasterizerState                       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.BlendState                            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState                     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask                            = UINT_MAX;
    psoDesc.PrimitiveTopologyType                 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                      = 1;
    psoDesc.RTVFormats[0]                         = m_BackBufferFormat;
    psoDesc.SampleDesc.Count                      = m_4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality                    = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat                             = m_DepthStencilFormat;

    m_PipelineState["no_texture"] = ComPtr<ID3D12PipelineState>();
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_PipelineState["no_texture"])));

    // for texture
    psoDesc.PS                 = m_PS["texture"];
    m_PipelineState["texture"] = ComPtr<ID3D12PipelineState>();
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_PipelineState["texture"])));

#if defined(_DEBUG)
    psoDesc.InputLayout           = {m_DebugInputLayout.data(),
                           static_cast<UINT>(m_DebugInputLayout.size())};
    psoDesc.VS                    = m_VS["debug"];
    psoDesc.PS                    = m_PS["debug"];
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    m_PipelineState["debug"]      = ComPtr<ID3D12PipelineState>();
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_PipelineState["debug"])));
#endif  // DEBUG
}

void D3D12GraphicsManager::InitializeBuffers(const Scene& scene) {
    // Initialize meshes buffer
    for (auto&& [key, pMesh] : scene.Meshes) {
        auto m      = std::make_shared<MeshBuffer>();
        m->material = pMesh->GetMaterial();
        for (auto&& inputLayout : m_InputLayout) {
            auto& vertexArray = pMesh->GetVertexPropertyArray(inputLayout.SemanticName);
            CreateVertexBuffer(vertexArray, m);
        }
        auto& indexArray = pMesh->GetIndexArray();
        m->indexCount    = indexArray.GetIndexCount();
        CreateIndexBuffer(indexArray, m);
        SetPrimitiveType(pMesh->GetPrimitiveType(), m);
        if (auto material = pMesh->GetMaterial().lock())
            if (material->GetBaseColor().ValueMap)
                m->pPSO = m_PipelineState["texture"];
            else
                m->pPSO = m_PipelineState["no_texture"];
        else
            m->pPSO = m_PipelineState["simple"];

        m_MeshBuffer[pMesh->GetGuid()] = m;
    }
    // Intialize textures buffer
    size_t i = 0;
    for (auto&& [key, material] : scene.Materials) {
        if (auto texture = material->GetBaseColor().ValueMap) {
            auto  guid       = texture->GetGuid();
            auto& image      = texture->GetTextureImage();
            m_Textures[guid] = CreateTextureBuffer(image, i);
            i++;
        }
    }
    // Initialize draw item
    for (auto&& [key, node] : scene.GeometryNodes) {
        if (node->Visible()) {
            auto geometry = scene.GetGeometry(node->GetSceneObjectRef());
            for (auto&& mesh : geometry->GetMeshes()) {
                if (auto pMesh = mesh.lock()) {
                    DrawItem d;
                    d.constantBufferIndex = m_DrawItems.size();
                    d.meshBuffer          = m_MeshBuffer[pMesh->GetGuid()];
                    d.node                = node;
                    d.numFramesDirty      = m_FrameResourceSize;
                    m_DrawItems.push_back(std::move(d));
                }
            }
        }
    }
}

void D3D12GraphicsManager::CreateVertexBuffer(
    const SceneObjectVertexArray&      vertexArray,
    const std::shared_ptr<MeshBuffer>& pMeshBuffer) {
    ThrowIfFailed(m_CommandAllocator->Reset());
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

    ComPtr<ID3D12Resource> pUploader;
    auto                   pVertexBuffer = d3dUtil::CreateDefaultBuffer(
        m_Device.Get(), m_CommandList.Get(), vertexArray.GetData(),
        vertexArray.GetDataSize(), pUploader);

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
    vbv.SizeInBytes    = vertexArray.GetDataSize();
    vbv.StrideInBytes =
        vertexArray.GetDataSize() / vertexArray.GetVertexCount();

    pMeshBuffer->vbv.push_back(vbv);
    pMeshBuffer->verticesBuffer.push_back(pVertexBuffer);
    m_Uploader.push_back(pUploader);

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
}

void D3D12GraphicsManager::CreateIndexBuffer(
    const SceneObjectIndexArray&       indexArray,
    const std::shared_ptr<MeshBuffer>& pMeshBuffer) {
    ThrowIfFailed(m_CommandAllocator->Reset());
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

    ComPtr<ID3D12Resource> pUploader;
    auto                   pIndexBuffer = d3dUtil::CreateDefaultBuffer(
        m_Device.Get(), m_CommandList.Get(), indexArray.GetData(),
        indexArray.GetDataSize(), pUploader);

    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
    ibv.Format         = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes    = indexArray.GetDataSize();

    pMeshBuffer->ibv           = ibv;
    pMeshBuffer->indicesBuffer = pIndexBuffer;
    m_Uploader.push_back(pUploader);

    ThrowIfFailed(m_CommandList->Close());

    ID3D12CommandList* cmdsLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
}

void D3D12GraphicsManager::SetPrimitiveType(
    const PrimitiveType&               primitiveType,
    const std::shared_ptr<MeshBuffer>& pMeshBuffer) {
    switch (primitiveType) {
        case PrimitiveType::LINE_LIST:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case PrimitiveType::LINE_LIST_ADJACENCY:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
            break;
        case PrimitiveType::TRI_LIST:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case PrimitiveType::TRI_LIST_ADJACENCY:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
            break;
        default:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
    }
}

std::shared_ptr<D3D12GraphicsManager::TextureBuffer>
D3D12GraphicsManager::CreateTextureBuffer(const Image& img, size_t srvOffset) {
    ThrowIfFailed(m_CommandAllocator->Reset());
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels           = 1;
    textureDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize    = 1;
    textureDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.SampleDesc.Count    = m_4xMsaaState ? 4 : 1;
    textureDesc.SampleDesc.Quality  = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    textureDesc.Width               = img.GetWidth();
    textureDesc.Height              = img.GetHeight();
    if (img.GetBitcount() == 32)
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    else if (img.GetBitcount() == 8)
        textureDesc.Format = DXGI_FORMAT_R8_UNORM;

    auto defaultheapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto uploadHeapProp  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format                          = textureDesc.Format;
    srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels             = 1;

    auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_CbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), m_SrvOffset + srvOffset,
        m_CbvSrvUavHeapSize);

    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> uploadHeap;

    // Create texture buffer

    ThrowIfFailed(m_Device->CreateCommittedResource(
        &defaultheapProp, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&texture)));

    // Create the GPU upload buffer.
    const UINT subresourceCount =
        textureDesc.DepthOrArraySize * textureDesc.MipLevels;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(
        texture.Get(), 0, subresourceCount);
    auto uploadeResourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &uploadHeapProp, D3D12_HEAP_FLAG_NONE, &uploadeResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData;
    textureData.pData      = img.getData();
    textureData.RowPitch   = img.GetPitch();
    textureData.SlicePitch = img.GetPitch() * img.GetHeight();
    UpdateSubresources(m_CommandList.Get(), texture.Get(),
                       uploadHeap.Get(), 0, 0, subresourceCount,
                       &textureData);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    m_CommandList->ResourceBarrier(1, &barrier);
    m_Device->CreateShaderResourceView(texture.Get(), &srvDesc, handle);
    auto textureBuffer        = std::make_shared<TextureBuffer>();
    textureBuffer->index      = m_Textures.size();
    textureBuffer->texture    = texture;
    textureBuffer->uploadHeap = uploadHeap;

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
    return textureBuffer;
}

void D3D12GraphicsManager::ClearShaders() {
    m_PipelineState.clear();

    m_VS.clear();
    m_PS.clear();
}

void D3D12GraphicsManager::ClearBuffers() {
    FlushCommandQueue();
    m_DrawItems.clear();
    m_MeshBuffer.clear();
    m_Textures.clear();
    ClearDebugBuffers();
    m_DebugMeshBuffer.clear();
    m_Uploader.clear();
}

void D3D12GraphicsManager::UpdateConstants() {
    GraphicsManager::UpdateConstants();
    auto currFR = m_FrameResource[m_CurrFrameResourceIndex].get();

    // Wait untill the frame is finished.
    m_CommandQueue->Signal(m_Fence.Get(), currFR->fence);
    if (currFR->fence != 0 && m_Fence->GetCompletedValue() < currFR->fence) {
        ThrowIfFailed(
            m_Fence->SetEventOnCompletion(currFR->fence, m_FenceEvent));
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }

    // Update frame resource

    currFR->UpdateFrameConstants(m_FrameConstants);
    for (auto&& d : m_DrawItems) {
        auto node = d.node.lock();
        if (!node) continue;

        if (node->Dirty()) {
            node->ClearDirty();
            // object need to be upadted for per frame resource
            d.numFramesDirty = m_FrameResourceSize;
        }
        if (d.numFramesDirty > 0) {
            ObjectConstants oc;
            oc.modelMatrix = *node->GetCalculatedTransform();

            if (auto material = d.meshBuffer->material.lock()) {
                auto& baseColor     = material->GetBaseColor();
                oc.baseColor        = baseColor.ValueMap ? vec4f(-1.0f) : baseColor.Value;
                auto& specularColor = material->GetSpecularColor();
                oc.specularColor =
                    specularColor.ValueMap ? vec4f(-1.0f) : specularColor.Value;
                oc.specularPower = material->GetSpecularPower().Value;
            }

            currFR->UpdateObjectConstants(d.constantBufferIndex, oc);
            d.numFramesDirty--;
        }
    }
#if defined(_DEBUG)
    for (auto&& d : m_DebugDrawItems) {
        if (auto node = d.node.lock()) {
            ObjectConstants oc;
            oc.modelMatrix = *node->GetCalculatedTransform();
            currFR->UpdateObjectConstants(d.constantBufferIndex, oc);
        }
    }
#endif
}

void D3D12GraphicsManager::RenderBuffers() {
    auto currFR = m_FrameResource[m_CurrFrameResourceIndex].get();

    PopulateCommandList();

    ID3D12CommandList* cmdsLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % m_FrameCount;

    currFR->fence = ++m_CurrFence;
    ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), m_CurrFence));
}

void D3D12GraphicsManager::PopulateCommandList() {
    auto fr = m_FrameResource[m_CurrFrameResourceIndex].get();
    ThrowIfFailed(fr->m_CommandAllocator->Reset());

    ThrowIfFailed(
        m_CommandList->Reset(fr->m_CommandAllocator.Get(), nullptr));

    // change state from presentation to waitting to render.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_CurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &barrier);

    m_CommandList->RSSetViewports(1, &m_Viewport);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    // clear buffer view
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_CommandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor,
                                         0, nullptr);
    m_CommandList->ClearDepthStencilView(
        DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    // render back buffer
    const D3D12_CPU_DESCRIPTOR_HANDLE& currBackBuffer = CurrentBackBufferView();
    const D3D12_CPU_DESCRIPTOR_HANDLE& dsv            = DepthStencilView();
    m_CommandList->OMSetRenderTargets(1, &currBackBuffer, false, &dsv);

    ID3D12DescriptorHeap* pDescriptorHeaps[] = {m_CbvSrvHeap.Get(),
                                                m_SamplerHeap.Get()};
    m_CommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps),
                                      pDescriptorHeaps);

    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    // Sampler
    m_CommandList->SetGraphicsRootDescriptorTable(
        3, m_SamplerHeap->GetGPUDescriptorHandleForHeapStart());
    // Frame Cbv
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(
        m_CbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
        m_FrameCBOffset + m_CurrFrameResourceIndex, m_CbvSrvUavHeapSize);
    m_CommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

    DrawRenderItems(m_CommandList.Get(), m_DrawItems);

#if defined(_DEBUG)
    DrawRenderItems(m_CommandList.Get(), m_DebugDrawItems);
#endif  // DEBUG

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_CurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_CommandList->Close());
}

void D3D12GraphicsManager::DrawRenderItems(
    ID3D12GraphicsCommandList4*  cmdList,
    const std::vector<DrawItem>& drawItems) {
    //
    auto         fr       = m_FrameResource[m_CurrFrameResourceIndex].get();
    const size_t cbv_size = fr->m_ObjCBUploader->GetElementCount();

    for (auto&& d : drawItems) {
        const auto& meshBuffer = d.meshBuffer;
        cmdList->SetPipelineState(meshBuffer->pPSO.Get());

        for (size_t i = 0; i < meshBuffer->vbv.size(); i++) {
            cmdList->IASetVertexBuffers(i, 1, &meshBuffer->vbv[i]);
        }

        cmdList->IASetIndexBuffer(&meshBuffer->ibv);

        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(
            m_CbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
            cbv_size * m_CurrFrameResourceIndex + d.constantBufferIndex,
            m_CbvSrvUavHeapSize);
        cmdList->SetGraphicsRootDescriptorTable(1, cbvHandle);

        // Texture
        if (auto material = meshBuffer->material.lock()) {
            if (auto& pTexture = material->GetBaseColor().ValueMap) {
                size_t offset =
                    m_SrvOffset + m_Textures[pTexture->GetGuid()]->index;

                auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                    m_CbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), offset,
                    m_CbvSrvUavHeapSize);
                cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
            }
        }
        cmdList->IASetPrimitiveTopology(meshBuffer->primitiveType);
        cmdList->DrawIndexedInstanced(meshBuffer->indexCount, 1, 0, 0, 0);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::CurrentBackBufferView()
    const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_RtvHeap->GetCPUDescriptorHandleForHeapStart(), m_CurrBackBuffer,
        m_RtvHeapSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::DepthStencilView() const {
    return m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3D12GraphicsManager::FlushCommandQueue() {
    m_CurrFence++;
    ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), m_CurrFence));
    if (m_Fence->GetCompletedValue() < m_CurrFence) {
        ThrowIfFailed(
            m_Fence->SetEventOnCompletion(m_CurrFence, m_FenceEvent));

        WaitForSingleObject(m_FenceEvent, INFINITE);
    }
}

#if defined(_DEBUG)
void D3D12GraphicsManager::RenderLine(const vec3f& from, const vec3f& to,
                                      const vec3f& color) {
    std::string name;
    if (color.r > 0)
        name = "debug_line-x";
    else if (color.b > 0)
        name = "debug_line-y";
    else
        name = "debug_line-z";

    if (m_DebugMeshBuffer.find(name) == m_DebugMeshBuffer.end()) {
        auto meshBuffer                 = std::make_shared<MeshBuffer>();
        meshBuffer->primitiveType       = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        std::vector<vec3f>     position = {{0, 0, 0}, {1, 0, 0}};
        std::vector<int>       index    = {0, 1};
        std::vector<vec3f>     colors(position.size(), color);
        SceneObjectVertexArray pos_array("POSITION", 0, VertexDataType::FLOAT3,
                                         position.data(), position.size());
        SceneObjectVertexArray color_array("COLOR", 0, VertexDataType::FLOAT3,
                                           colors.data(), colors.size());
        CreateVertexBuffer(pos_array, meshBuffer);
        CreateVertexBuffer(color_array, meshBuffer);
        SceneObjectIndexArray index_array(0, IndexDataType::INT32,
                                          index.data(), index.size());
        CreateIndexBuffer(index_array, meshBuffer);
        meshBuffer->indexCount  = index.size();
        meshBuffer->pPSO        = m_PipelineState["debug"];
        m_DebugMeshBuffer[name] = meshBuffer;
    }

    vec3f v1          = to - from;
    vec3f v2          = {Length(v1), 0, 0};
    vec3f rotate_axis = v1 + v2;
    mat4f transform   = scale(mat4f(1.0f), vec3f(Length(v1)));
    transform         = rotate(transform, radians(180.0f), rotate_axis);
    transform         = translate(transform, from);

    auto node = std::make_shared<SceneGeometryNode>();
    node->AppendTransform(
        std::make_shared<SceneObjectTransform>(transform));
    m_DebugNode.push_back(node);

    DrawItem d;
    d.node                = node;
    d.meshBuffer          = m_DebugMeshBuffer[name];
    d.constantBufferIndex = m_DrawItems.size() + m_DebugDrawItems.size();
    d.numFramesDirty      = m_FrameResourceSize;
    m_DebugDrawItems.push_back(d);
}

void D3D12GraphicsManager::RenderBox(const vec3f& bbMin, const vec3f& bbMax,
                                     const vec3f& color) {
    if (m_DebugMeshBuffer.find("debug_box") == m_DebugMeshBuffer.end()) {
        auto               meshBuffer = std::make_shared<MeshBuffer>();
        std::vector<vec3f> position   = {
            {-1, -1, -1},
            {1, -1, -1},
            {1, 1, -1},
            {-1, 1, -1},
            {-1, -1, 1},
            {1, -1, 1},
            {1, 1, 1},
            {-1, 1, 1},
        };
        std::vector<int> index    = {0, 1, 2, 3, 0, 4, 5, 1,
                                  5, 6, 2, 6, 7, 3, 7, 4};
        meshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        std::vector<vec3f>     colors(position.size(), color);
        SceneObjectVertexArray pos_array("POSITION", 0, VertexDataType::FLOAT3,
                                         position.data(), position.size());
        SceneObjectVertexArray color_array("COLOR", 0, VertexDataType::FLOAT3,
                                           colors.data(), colors.size());
        CreateVertexBuffer(pos_array, meshBuffer);
        CreateVertexBuffer(color_array, meshBuffer);
        SceneObjectIndexArray index_array(0, IndexDataType::INT32,
                                          index.data(), index.size());
        CreateIndexBuffer(index_array, meshBuffer);
        meshBuffer->indexCount = index.size();

        meshBuffer->pPSO               = m_PipelineState["debug"];
        m_DebugMeshBuffer["debug_box"] = meshBuffer;
    }
    mat4f transform = translate(scale(mat4f(1.0f), 0.5 * (bbMax - bbMin)),
                                0.5 * (bbMax + bbMin));

    auto node = std::make_shared<SceneGeometryNode>();
    node->AppendTransform(
        std::make_shared<SceneObjectTransform>(transform));
    m_DebugNode.push_back(node);

    DrawItem d;
    d.node                = node;
    d.meshBuffer          = m_DebugMeshBuffer["debug_box"];
    d.constantBufferIndex = m_DrawItems.size() + m_DebugDrawItems.size();
    d.numFramesDirty      = m_FrameResourceSize;
    m_DebugDrawItems.push_back(d);
}

void D3D12GraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {
}

void D3D12GraphicsManager::ClearDebugBuffers() {
    m_DebugNode.clear();
    m_DebugDrawItems.clear();
}

#endif  // DEBUG

}  // namespace Hitagi