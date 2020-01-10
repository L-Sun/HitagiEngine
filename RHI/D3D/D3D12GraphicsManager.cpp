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

void D3D12GraphicsManager::Finalize() { GraphicsManager::Finalize(); }

void D3D12GraphicsManager::Draw() {
    m_nCurrFrameResourceIndex =
        (m_nCurrFrameResourceIndex + 1) % m_nFrameResourceSize;
    GraphicsManager::Draw();
}

void D3D12GraphicsManager::Clear() { GraphicsManager::Clear(); }

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
        ThrowIfFailed(m_pDevice->CreateFence(
            m_nCurrFence, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
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

    // Set viewport and scissor rect.
    {
        m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, config.screenWidth,
                                      config.screenHeight);
        m_scissorRect =
            CD3DX12_RECT(0, 0, config.screenWidth, config.screenHeight);
    }

    return 0;
}

void D3D12GraphicsManager::CreateSwapChain() {
    m_pSwapChain.Reset();
    ComPtr<IDXGISwapChain1> swapChain;
    DXGI_SWAP_CHAIN_DESC1   swapChainDesc = {};
    swapChainDesc.BufferCount             = m_nFrameCount;
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
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&m_pCommandList)));
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_pCommandList->Close());
}

void D3D12GraphicsManager::InitializeBuffers(const Scene& scene) {
    m_pScene = &scene;
    for (auto&& [key, pGeometryNode] : m_pScene->GeometryNodes) {
        if (pGeometryNode && pGeometryNode->Visible()) {
            auto pGeometry =
                m_pScene->GetGeometry(pGeometryNode->GetSceneObjectRef());
            assert(pGeometry);
            auto pMesh = pGeometry->GetMesh().lock();
            if (!pMesh) continue;

            // LOD
            auto& index_array    = pMesh->GetIndexArray(0);
            auto  material_index = index_array.GetMaterialIndex();
            auto  material_key = pGeometryNode->GetMaterialRef(material_index);
            auto  material     = scene.GetMaterial(material_key);

            D3D12DrawBatchContext dbc;
            dbc.node           = pGeometryNode;
            dbc.count          = index_array.GetIndexCount();
            dbc.property_count = pMesh->GetVertexPropertiesCount();
            dbc.material       = material;
            dbc.numFramesDirty = m_nFrameResourceSize;

            m_drawBatchContext.push_back(dbc);
        }
    }

    CreateDescriptorHeaps();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateFrameResource();
    CreateRootSignature();
    CreateConstantBuffer();
    CreateTextureBuffer();
    CreateSampler();
}
void D3D12GraphicsManager::CreateDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask                   = 0;
    rtvHeapDesc.NumDescriptors             = m_nFrameCount;
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
    for (unsigned i = 0; i < m_nFrameCount; i++) {
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
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear,
        IID_PPV_ARGS(&m_pDepthStencilBuffer)));

    m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr,
                                      DepthStencilView());

    // Create CBV SRV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC CbvSrvHeapDesc = {};
    CbvSrvHeapDesc.Flags    = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CbvSrvHeapDesc.NodeMask = 0;
    // per frame have n objects constant buffer descriptor and 1 frame constant
    // buffer descriptor
    CbvSrvHeapDesc.NumDescriptors =
        m_nFrameResourceSize *
        (m_drawBatchContext.size() + 1 + m_pScene->Materials.size());
    CbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(
        &CbvSrvHeapDesc, IID_PPV_ARGS(&m_pCbvSrvHeap)));

    // Use the last n descriptor as the frame constant buffer descriptor
    m_nFrameCBOffset = m_nFrameResourceSize * m_drawBatchContext.size();
    m_nSrvOffset     = m_nFrameResourceSize * (m_drawBatchContext.size() + 1);

    // Sampler descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC samplerDescHeap = {};
    samplerDescHeap.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    samplerDescHeap.NumDescriptors = 1;
    samplerDescHeap.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(
        &samplerDescHeap, IID_PPV_ARGS(&m_pSamplerHeap)));
}

void D3D12GraphicsManager::CreateVertexBuffer() {
    ThrowIfFailed(m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr));

    for (auto&& dbc : m_drawBatchContext) {
        auto pGeometry = m_pScene->GetGeometry(dbc.node->GetSceneObjectRef());
        auto pMesh     = pGeometry->GetMesh().lock();
        if (!pMesh) continue;
        size_t vertexPropertiesCount = pMesh->GetVertexPropertiesCount();

        for (size_t i = 0; i < vertexPropertiesCount; i++) {
            const SceneObjectVertexArray& vertexArray =
                pMesh->GetVertexPropertyArray(i);
            ComPtr<ID3D12Resource> pUploader;
            auto                   pVertexBuffer = d3dUtil::CreateDefaultBuffer(
                m_pDevice.Get(), m_pCommandList.Get(), vertexArray.GetData(),
                vertexArray.GetDataSize(), pUploader);

            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
            vbv.SizeInBytes    = vertexArray.GetDataSize();
            vbv.StrideInBytes =
                vertexArray.GetDataSize() / vertexArray.GetVertexCount();
            m_vertexBufferView.push_back(vbv);
            m_Buffers.push_back(pVertexBuffer);
            m_Buffers.push_back(pUploader);
        }
    }

    ThrowIfFailed(m_pCommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
}

void D3D12GraphicsManager::CreateIndexBuffer() {
    ThrowIfFailed(m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr));

    for (auto&& dbc : m_drawBatchContext) {
        auto pGeometry = m_pScene->GetGeometry(dbc.node->GetSceneObjectRef());
        auto pMesh     = pGeometry->GetMesh().lock();
        if (!pMesh) continue;
        // LOD
        const SceneObjectIndexArray& indexArray = pMesh->GetIndexArray(0);

        ComPtr<ID3D12Resource> pUploader;
        auto                   pIndexBuffer = d3dUtil::CreateDefaultBuffer(
            m_pDevice.Get(), m_pCommandList.Get(), indexArray.GetData(),
            indexArray.GetDataSize(), pUploader);

        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
        ibv.Format         = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes    = indexArray.GetDataSize();
        m_indexBufferView.push_back(ibv);

        m_Buffers.push_back(pIndexBuffer);
        m_Buffers.push_back(pUploader);
    }

    ThrowIfFailed(m_pCommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
}

void D3D12GraphicsManager::CreateFrameResource() {
    for (size_t i = 0; i < m_nFrameResourceSize; i++) {
        m_frameResource.push_back(
            make_unique<FR>(m_pDevice.Get(), m_drawBatchContext.size()));
    }
}

void D3D12GraphicsManager::CreateRootSignature() {
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(m_pDevice->CheckFeatureSupport(
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
    ThrowIfFailed(m_pDevice->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_pRootSignature)));
}

void D3D12GraphicsManager::CreateConstantBuffer() {
    // -----------------------------------------------------------
    // |0|1|2|...|n-1|     |n|n+1|...|2n-1|...... |mn-1|
    // 1st frame resrouce  2nd frame resource     end of nst frame
    // for object          for object             for object
    // |mn|mn+1|...|mn+m-1|
    // for m frame consant buffer
    // |m(n+1)|...|m(n+1)+k-1|......|m(n+1+k)-1|
    // 1st frame for srv            end of nst frame for srv
    // -----------------------------------------------------------
    auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pCbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
    // Create object constant buffer
    for (size_t i = 0; i < m_nFrameResourceSize; i++) {
        auto                            fr = m_frameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDsec;
        cbDsec.SizeInBytes = fr->m_pObjCBUploader->GetCBByteSize();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress =
            fr->m_pObjCBUploader->Resource()->GetGPUVirtualAddress();

        for (size_t j = 0; j < fr->m_pObjCBUploader->GetElementCount(); j++) {
            cbDsec.BufferLocation = cbAddress;
            m_pDevice->CreateConstantBufferView(&cbDsec, handle);
            cbAddress += cbDsec.SizeInBytes;
            handle.Offset(m_nCbvSrvUavHeapSize);
        }
    }
    // Create frame constant buffer, handle have been at the begin of
    // last m descriptor area
    for (size_t i = 0; i < m_nFrameResourceSize; i++) {
        auto                            fr = m_frameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc;
        cbDesc.SizeInBytes = fr->m_pFrameCBUploader->GetCBByteSize();
        cbDesc.BufferLocation =
            fr->m_pFrameCBUploader->Resource()->GetGPUVirtualAddress();
        m_pDevice->CreateConstantBufferView(&cbDesc, handle);
        handle.Offset(m_nCbvSrvUavHeapSize);
    }
}

void D3D12GraphicsManager::CreateTextureBuffer() {
    // |m(n+1)|...|m(n+1)+k-1|......|m(n+1+k)-1|
    // 1st frame for srv            end of nst frame for srv
    ThrowIfFailed(m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr));

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels           = 1;
    textureDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize    = 1;
    textureDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.SampleDesc.Count    = m_b4xMsaaState ? 4 : 1;
    textureDesc.SampleDesc.Quality =
        m_b4xMsaaState ? (m_n4xMsaaQuality - 1) : 0;

    auto defaultheapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto uploadHeapProp  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format                  = textureDesc.Format;
    srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels     = 1;

    auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), m_nSrvOffset,
        m_nCbvSrvUavHeapSize);

    for (auto&& fr : m_frameResource) {
        for (auto&& [key, pMaterial] : m_pScene->Materials) {
            const auto& color = pMaterial->GetBaseColor();
            if (auto pTexture = color.ValueMap) {
                if (fr->m_textures.find(pTexture->GetName()) ==
                    fr->m_textures.end()) {
                    auto img = pTexture->GetTextureImage();
                    // Create texture buffer
                    ComPtr<ID3D12Resource> texture;
                    ComPtr<ID3D12Resource> uploadHeap;

                    textureDesc.Width  = img.Width;
                    textureDesc.Height = img.Height;

                    ThrowIfFailed(m_pDevice->CreateCommittedResource(
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
                    ThrowIfFailed(m_pDevice->CreateCommittedResource(
                        &uploadHeapProp, D3D12_HEAP_FLAG_NONE,
                        &uploadeResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                        nullptr, IID_PPV_ARGS(&uploadHeap)));

                    D3D12_SUBRESOURCE_DATA textureData;
                    textureData.pData      = img.data;
                    textureData.RowPitch   = img.pitch;
                    textureData.SlicePitch = img.pitch * img.Height;
                    UpdateSubresources(m_pCommandList.Get(), texture.Get(),
                                       uploadHeap.Get(), 0, 0, subresourceCount,
                                       &textureData);
                    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_GENERIC_READ);
                    m_pCommandList->ResourceBarrier(1, &barrier);

                    m_pDevice->CreateShaderResourceView(texture.Get(), &srvDesc,
                                                        handle);
                    handle.Offset(m_nCbvSrvUavHeapSize);

                    fr->m_textures.insert(
                        {pTexture->GetName(),
                         {fr->m_textures.size(), texture, uploadHeap}});
                }
            }
        }
    }
    ThrowIfFailed(m_pCommandList->Close());
    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
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

    m_pDevice->CreateSampler(
        &samplerDesc, m_pSamplerHeap->GetCPUDescriptorHandleForHeapStart());
}

// this is the function that loads and prepares the shaders
bool D3D12GraphicsManager::InitializeShaders() {
    Buffer v_shader =
               g_pAssetLoader->SyncOpenAndReadBinary("Shaders/simple_vs.cso"),
           p1_shader =
               g_pAssetLoader->SyncOpenAndReadBinary("Shaders/simple_ps_1.cso");

    m_VS.push_back(
        CD3DX12_SHADER_BYTECODE(v_shader.GetData(), v_shader.GetDataSize()));
    m_PS.push_back(
        CD3DX12_SHADER_BYTECODE(p1_shader.GetData(), p1_shader.GetDataSize()));

    Buffer p2_shader =
        g_pAssetLoader->SyncOpenAndReadBinary("Shaders/simple_ps_2.cso");
    m_PS.push_back(
        CD3DX12_SHADER_BYTECODE(p2_shader.GetData(), p2_shader.GetDataSize()));

    // Define the vertex input layout, becase vertices storage is
    m_inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    BuildPipelineStateObject();

    return true;
}

void D3D12GraphicsManager::BuildPipelineStateObject() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout    = {m_inputLayout.data(), (UINT)m_inputLayout.size()};
    psoDesc.pRootSignature = m_pRootSignature.Get();
    psoDesc.VS             = m_VS[0];
    psoDesc.PS             = m_PS[0];
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = m_BackBufferFormat;
    psoDesc.SampleDesc.Count      = m_b4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality    = m_b4xMsaaState ? (m_n4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat             = m_DepthStencilFormat;

    m_pPipelineState.push_back(ComPtr<ID3D12PipelineState>());
    ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_pPipelineState[0])));

    // for texture
    psoDesc.PS = m_PS[1];
    m_pPipelineState.push_back(ComPtr<ID3D12PipelineState>());
    ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_pPipelineState[1])));
}

void D3D12GraphicsManager::ClearShaders() {}

void D3D12GraphicsManager::ClearBuffers() {}

void D3D12GraphicsManager::UpdateConstants() {
    GraphicsManager::UpdateConstants();
    auto currFR = m_frameResource[m_nCurrFrameResourceIndex].get();

    // Wait untill the frame is finished.
    m_pCommandQueue->Signal(m_pFence.Get(), currFR->fence);
    if (currFR->fence != 0 && m_pFence->GetCompletedValue() < currFR->fence) {
        ThrowIfFailed(
            m_pFence->SetEventOnCompletion(currFR->fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Update frame resource
    currFR->UpdateFrameConstants(m_frameConstants);
    int32_t i = 0;
    for (auto&& dbc : m_drawBatchContext) {
        if (dbc.node->Dirty()) {
            dbc.node->ClearDirty();
            // object need to be upadted for per frame resource
            dbc.numFramesDirty = m_nFrameResourceSize;
        }
        if (dbc.numFramesDirty > 0) {
            ObjectConstants oc;
            oc.modelMatrix = *dbc.node->GetCalculatedTransform();

            if (dbc.material) {
                const Color* pColor = &dbc.material->GetBaseColor();
                oc.baseColor = pColor->ValueMap ? vec4(-1.0f) : pColor->Value;
                pColor       = &dbc.material->GetSpecularColor();
                oc.specularColor =
                    pColor->ValueMap ? vec4(-1.0f) : pColor->Value;
                oc.specularPower = dbc.material->GetSpecularPower().Value;
            }

            currFR->UpdateObjectConstants(i, oc);
            dbc.numFramesDirty--;
        }
        i++;
    }
}

void D3D12GraphicsManager::RenderBuffers() {
    auto currFR = m_frameResource[m_nCurrFrameResourceIndex].get();

    PopulateCommandList();

    ID3D12CommandList* cmdsLists[] = {m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    ThrowIfFailed(m_pSwapChain->Present(0, 0));
    m_nCurrBackBuffer = (m_nCurrBackBuffer + 1) % m_nFrameCount;

    currFR->fence = ++m_nCurrFence;
    ThrowIfFailed(m_pCommandQueue->Signal(m_pFence.Get(), m_nCurrFence));
}

void D3D12GraphicsManager::PopulateCommandList() {
    auto fr = m_frameResource[m_nCurrFrameResourceIndex].get();
    ThrowIfFailed(fr->m_pCommandAllocator->Reset());

    ThrowIfFailed(
        m_pCommandList->Reset(fr->m_pCommandAllocator.Get(), nullptr));

    m_pCommandList->SetPipelineState(m_pPipelineState[0].Get());

    // change state from presentation to waitting to render.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pRenderTargets[m_nCurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_pCommandList->ResourceBarrier(1, &barrier);

    m_pCommandList->RSSetViewports(1, &m_viewport);
    m_pCommandList->RSSetScissorRects(1, &m_scissorRect);

    // clear buffer view
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_pCommandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor,
                                          0, nullptr);
    m_pCommandList->ClearDepthStencilView(
        DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    // render back buffer
    const D3D12_CPU_DESCRIPTOR_HANDLE& currBackBuffer = CurrentBackBufferView();
    const D3D12_CPU_DESCRIPTOR_HANDLE& dsv            = DepthStencilView();
    m_pCommandList->OMSetRenderTargets(1, &currBackBuffer, false, &dsv);

    ID3D12DescriptorHeap* pDescriptorHeaps[] = {m_pCbvSrvHeap.Get(),
                                                m_pSamplerHeap.Get()};
    m_pCommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps),
                                       pDescriptorHeaps);

    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    // Sampler
    m_pCommandList->SetGraphicsRootDescriptorTable(
        3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
    // Frame Cbv
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(
        m_pCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
        m_nFrameCBOffset + m_nCurrFrameResourceIndex, m_nCbvSrvUavHeapSize);
    m_pCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

    cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_pCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
        // Offset of the first object in constant buffer heap at current frame
        fr->m_pObjCBUploader->GetElementCount() * m_nCurrFrameResourceIndex,
        m_nCbvSrvUavHeapSize);
    size_t i          = 0;
    size_t vbv_offset = 0;
    for (auto&& dbc : m_drawBatchContext) {
        for (size_t j = 0; j < dbc.property_count; j++) {
            m_pCommandList->IASetVertexBuffers(
                j, 1, &m_vertexBufferView[vbv_offset++]);
        }

        m_pCommandList->SetGraphicsRootDescriptorTable(1, cbvHandle);
        cbvHandle.Offset(m_nCbvSrvUavHeapSize);
        m_pCommandList->IASetIndexBuffer(&m_indexBufferView[i++]);

        // Texture
        if (dbc.material) {
            if (auto& pTexture = dbc.material->GetBaseColor().ValueMap) {
                m_pCommandList->SetPipelineState(m_pPipelineState[1].Get());
                size_t offset =
                    m_nSrvOffset +
                    m_nCurrFrameResourceIndex * fr->m_textures.size() +
                    fr->m_textures[pTexture->GetName()].index;
                auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                    m_pCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), offset,
                    m_nCbvSrvUavHeapSize);
                m_pCommandList->SetGraphicsRootDescriptorTable(2, srvHandle);
            }
        }

        m_pCommandList->DrawIndexedInstanced(dbc.count, 1, 0, 0, 0);
    }

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

void D3D12GraphicsManager::FlushCommandQueue() {
    m_nCurrFence++;
    ThrowIfFailed(m_pCommandQueue->Signal(m_pFence.Get(), m_nCurrFence));
    if (m_pFence->GetCompletedValue() < m_nCurrFence) {
        ThrowIfFailed(
            m_pFence->SetEventOnCompletion(m_nCurrFence, m_fenceEvent));

        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}