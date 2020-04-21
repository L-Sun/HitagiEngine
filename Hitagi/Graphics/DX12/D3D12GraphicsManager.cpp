#include "D3D12GraphicsManager.hpp"

#include "WindowsApplication.hpp"

namespace Hitagi::Graphics::DX12 {

int D3D12GraphicsManager::Initialize() {
    int result = GraphicsManager::Initialize();
    result     = InitD3D();
    return result;
}

void D3D12GraphicsManager::Finalize() { GraphicsManager::Finalize(); }

void D3D12GraphicsManager::Draw() {
    m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % m_FrameResourceSize;
    GraphicsManager::Draw();
}

void D3D12GraphicsManager::Clear() { GraphicsManager::Clear(); }

int D3D12GraphicsManager::InitD3D() {
    auto config = g_App->GetConfiguration();

    unsigned dxgiFactoryFlags = 0;

#if defined(_DEBUG)

    // Enable d3d12 debug layer.
    {
        Microsoft::WRL::ComPtr<ID3D12Debug3> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif  // DEBUG
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_DxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)))) {
            Microsoft::WRL::ComPtr<IDXGIAdapter4> pWarpaAdapter;
            ThrowIfFailed(m_DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpaAdapter)));
            ThrowIfFailed(D3D12CreateDevice(pWarpaAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));
        }
    }

    // Detect 4x MSAA
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Flags            = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.Format           = m_BackBufferFormat;
        msQualityLevels.NumQualityLevels = 0;
        msQualityLevels.SampleCount      = 4;
        ThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels,
                                                    sizeof(msQualityLevels)));

        m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
        assert(m_4xMsaaQuality > 0 && "Unexpected MSAA qulity level.");
    }

    // Initalize LinearAllocator
    m_LinearAllocator.Initalize(m_Device.Get());

    // Create command queue, allocator and list.
    m_CommandListManager.Initialize(m_Device.Get());
    CreateSwapChain();

    // Set viewport and scissor rect.
    {
        m_Viewport    = CD3DX12_VIEWPORT(0.0f, 0.0f, config.screenWidth, config.screenHeight);
        m_ScissorRect = CD3DX12_RECT(0, 0, config.screenWidth, config.screenHeight);
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
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    DXGI_SWAP_CHAIN_DESC1                   swapChainDesc = {};
    swapChainDesc.BufferCount                             = m_FrameCount;
    swapChainDesc.Width                                   = g_App->GetConfiguration().screenWidth;
    swapChainDesc.Height                                  = g_App->GetConfiguration().screenHeight;
    swapChainDesc.Format                                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage                             = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Scaling                                 = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect                              = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode                               = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count                        = m_4xMsaaState ? 4 : 1;
    swapChainDesc.SampleDesc.Quality                      = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    swapChainDesc.Flags                                   = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    HWND window       = reinterpret_cast<WindowsApplication*>(g_App.get())->GetMainWindow();
    auto commandQueue = m_CommandListManager.GetGraphicsQueue().GetCommandQueue();
    ThrowIfFailed(
        m_DxgiFactory->CreateSwapChainForHwnd(commandQueue, window, &swapChainDesc, nullptr, nullptr, &swapChain));

    ThrowIfFailed(swapChain.As(&m_SwapChain));
    ThrowIfFailed(m_DxgiFactory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
}

void D3D12GraphicsManager::CreateDescriptorHeaps() {
    for (int type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; type++) {
        m_DescriptorAllocator[type].Initialize(m_Device.Get(), static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
    }

    // Create rtv.
    m_RtvDescriptors = m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(m_FrameCount);
    for (unsigned i = 0; i < m_FrameCount; i++) {
        ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
        m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, m_RtvDescriptors.GetDescriptorHandle(i));
    }
    m_DsvDescriptors = m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate();

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
    ThrowIfFailed(m_Device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &dsvDesc,
                                                    D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear,
                                                    IID_PPV_ARGS(&m_DepthStencilBuffer)));

    m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, DepthStencilView());

    // Create CBV Descriptor Heap
    m_CbvSrvDescriptors = m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(
        m_FrameResourceSize * (m_MaxObjects + 1) + m_MaxTextures);

    // Use the last n descriptor as the frame constant buffer descriptor
    m_FrameCBOffset = m_FrameResourceSize * m_MaxObjects;
    m_SrvOffset     = m_FrameResourceSize * (m_MaxObjects + 1);

    m_SamplerDescriptors = m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate();
}

void D3D12GraphicsManager::CreateFrameResource() {
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        m_FrameResource.push_back(std::make_unique<FR>(m_LinearAllocator, m_MaxObjects));
    }
}

void D3D12GraphicsManager::CreateRootSignature() {
    m_RootSignature.Reset(4, 0);
    m_RootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1);
    m_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    m_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    m_RootSignature.Finalize(m_Device.Get(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                             featureData.HighestVersion);
}

void D3D12GraphicsManager::CreateConstantBuffer() {
    // -----------------------------------------------------------
    // |0|1|2|...|n-1|     |n|n+1|...|2n-1|...... |mn-1|
    // 1st frame resrouce  2nd frame resource     end of nst frame
    // for object          for object             for object
    // |mn|mn+1|...|mn+m-1|
    // for m frame consant buffer
    // -----------------------------------------------------------

    // Create object constant buffer
    uint32_t offset = 0;
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        auto                            fr = m_FrameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDsec;
        cbDsec.SizeInBytes                  = fr->ObjectSize();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = fr->m_ObjectConstantBuffer.GpuPtr;

        for (size_t j = 0; j < fr->m_NumObjects; j++) {
            cbDsec.BufferLocation = cbAddress;
            m_Device->CreateConstantBufferView(&cbDsec, m_CbvSrvDescriptors.GetDescriptorHandle(offset++));
            cbAddress += cbDsec.SizeInBytes;
        }
    }
    // Create frame constant buffer, handle have been at the begin of
    // last m descriptor area
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        auto                            fr = m_FrameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc;
        cbDesc.SizeInBytes    = fr->FrameConstantSize();
        cbDesc.BufferLocation = fr->m_FrameConstantBuffer.GpuPtr;
        m_Device->CreateConstantBufferView(&cbDesc, m_CbvSrvDescriptors.GetDescriptorHandle(offset++));
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

    m_Device->CreateSampler(&samplerDesc, m_SamplerDescriptors.GetDescriptorHandle());
}

bool D3D12GraphicsManager::InitializeShaders() {
    Core::Buffer v_shader  = g_FileIOManager->SyncOpenAndReadBinary("Asset/Shaders/simple_vs.cso"),
                 p1_shader = g_FileIOManager->SyncOpenAndReadBinary("Asset/Shaders/simple_ps_1.cso");

    m_ShaderManager.LoadShader("Asset/Shaders/simple_vs.cso", ShaderType::VERTEX, "simple");
    m_ShaderManager.LoadShader("Asset/Shaders/simple_ps_1.cso", ShaderType::PIXEL, "no_texture");
    m_ShaderManager.LoadShader("Asset/Shaders/simple_ps_2.cso", ShaderType::PIXEL, "texture");

    // Define the vertex input layout, becase vertices storage is
    m_InputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                     {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                     {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

#if defined(_DEBUG)
    m_ShaderManager.LoadShader("Asset/Shaders/debug_vs.cso", ShaderType::VERTEX, "debug");
    m_ShaderManager.LoadShader("Asset/Shaders/debug_ps.cso", ShaderType::PIXEL, "debug");

    m_DebugInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0},
                          {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0}};

#endif  // DEBUG

    BuildPipelineStateObject();

    return true;
}

void D3D12GraphicsManager::BuildPipelineStateObject() {
    auto rasterizerDesc                  = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerDesc.FrontCounterClockwise = true;
    auto blendDesc                       = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto depthStencilDesc                = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    m_GraphicsPSO.insert({"no_texture", GraphicsPSO()});
    m_GraphicsPSO["no_texture"].SetInputLayout(m_InputLayout);
    m_GraphicsPSO["no_texture"].SetRootSignature(m_RootSignature);
    m_GraphicsPSO["no_texture"].SetVertexShader(m_ShaderManager.GetVertexShader("simple"));
    m_GraphicsPSO["no_texture"].SetPixelShader(m_ShaderManager.GetPixelShader("no_texture"));
    m_GraphicsPSO["no_texture"].SetRasterizerState(rasterizerDesc);
    m_GraphicsPSO["no_texture"].SetBlendState(blendDesc);
    m_GraphicsPSO["no_texture"].SetDepthStencilState(depthStencilDesc);
    m_GraphicsPSO["no_texture"].SetSampleMask(UINT_MAX);
    m_GraphicsPSO["no_texture"].SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_GraphicsPSO["no_texture"].SetRenderTargetFormats(
        {m_BackBufferFormat}, m_DepthStencilFormat, m_4xMsaaState ? 4 : 1, m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0);
    m_GraphicsPSO["no_texture"].Finalize(m_Device.Get());

    m_GraphicsPSO.insert({"texture", GraphicsPSO()});
    m_GraphicsPSO["texture"].SetInputLayout(m_InputLayout);
    m_GraphicsPSO["texture"].SetRootSignature(m_RootSignature);
    m_GraphicsPSO["texture"].SetVertexShader(m_ShaderManager.GetVertexShader("simple"));
    m_GraphicsPSO["texture"].SetPixelShader(m_ShaderManager.GetPixelShader("texture"));
    m_GraphicsPSO["texture"].SetRasterizerState(rasterizerDesc);
    m_GraphicsPSO["texture"].SetBlendState(blendDesc);
    m_GraphicsPSO["texture"].SetDepthStencilState(depthStencilDesc);
    m_GraphicsPSO["texture"].SetSampleMask(UINT_MAX);
    m_GraphicsPSO["texture"].SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_GraphicsPSO["texture"].SetRenderTargetFormats({m_BackBufferFormat}, m_DepthStencilFormat, m_4xMsaaState ? 4 : 1,
                                                    m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0);
    m_GraphicsPSO["texture"].Finalize(m_Device.Get());

#if defined(_DEBUG)
    m_GraphicsPSO.insert({"debug", GraphicsPSO()});
    m_GraphicsPSO["debug"].SetInputLayout(m_DebugInputLayout);
    m_GraphicsPSO["debug"].SetRootSignature(m_RootSignature);
    m_GraphicsPSO["debug"].SetVertexShader(m_ShaderManager.GetVertexShader("debug"));
    m_GraphicsPSO["debug"].SetPixelShader(m_ShaderManager.GetPixelShader("debug"));
    m_GraphicsPSO["debug"].SetRasterizerState(rasterizerDesc);
    m_GraphicsPSO["debug"].SetBlendState(blendDesc);
    m_GraphicsPSO["debug"].SetDepthStencilState(depthStencilDesc);
    m_GraphicsPSO["debug"].SetSampleMask(UINT_MAX);
    m_GraphicsPSO["debug"].SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    m_GraphicsPSO["debug"].SetRenderTargetFormats({m_BackBufferFormat}, m_DepthStencilFormat, m_4xMsaaState ? 4 : 1,
                                                  m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0);
    m_GraphicsPSO["debug"].Finalize(m_Device.Get());
#endif  // DEBUG
}

void D3D12GraphicsManager::InitializeBuffers(const Resource::Scene& scene) {
    // Initialize meshes buffer
    for (auto&& [key, pMesh] : scene.Meshes) {
        auto m      = std::make_shared<MeshInfo>();
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
                m->psoName = "texture";
            else
                m->psoName = "no_texture";
        else
            m->psoName = "simple";

        m_Meshes[pMesh->GetGuid()] = m;
    }
    // Intialize textures buffer
    size_t i = 0;
    for (auto&& [key, material] : scene.Materials) {
        if (auto texture = material->GetBaseColor().ValueMap) {
            auto  guid  = texture->GetGuid();
            auto& image = texture->GetTextureImage();
            m_Textures.insert({guid, CreateTextureBuffer(image, i)});
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
                    d.meshBuffer          = m_Meshes[pMesh->GetGuid()];
                    d.node                = node;
                    d.numFramesDirty      = m_FrameResourceSize;
                    m_DrawItems.push_back(std::move(d));
                }
            }
        }
    }
}

void D3D12GraphicsManager::CreateVertexBuffer(const Resource::SceneObjectVertexArray& vertexArray,
                                              std::shared_ptr<MeshInfo>               pMeshBuffer) {
    CommandContext context(m_CommandListManager, D3D12_COMMAND_LIST_TYPE_DIRECT);
    GpuBuffer vb(m_Device.Get(), vertexArray.GetVertexCount(), vertexArray.GetDataSize() / vertexArray.GetVertexCount(),
                 vertexArray.GetData(), &context);
    pMeshBuffer->vbv.push_back(vb.VertexBufferView());
    m_Buffers.push_back(vb);
}

void D3D12GraphicsManager::CreateIndexBuffer(const Resource::SceneObjectIndexArray& indexArray,
                                             std::shared_ptr<MeshInfo>              pMeshBuffer) {
    CommandContext context(m_CommandListManager, D3D12_COMMAND_LIST_TYPE_DIRECT);
    GpuBuffer      ib(m_Device.Get(), indexArray.GetIndexCount(), indexArray.GetDataSize() / indexArray.GetIndexCount(),
                 indexArray.GetData(), &context);
    pMeshBuffer->ibv = ib.IndexBufferView();
    m_Buffers.push_back(ib);
}

void D3D12GraphicsManager::SetPrimitiveType(const Resource::PrimitiveType& primitiveType,
                                            std::shared_ptr<MeshInfo>      pMeshBuffer) {
    switch (primitiveType) {
        case Resource::PrimitiveType::LINE_LIST:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case Resource::PrimitiveType::LINE_LIST_ADJACENCY:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
            break;
        case Resource::PrimitiveType::TRI_LIST:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case Resource::PrimitiveType::TRI_LIST_ADJACENCY:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
            break;
        default:
            pMeshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
    }
}

TextureBuffer D3D12GraphicsManager::CreateTextureBuffer(const Resource::Image& image, size_t srvOffset) {
    auto handle = m_CbvSrvDescriptors.GetDescriptorHandle(m_SrvOffset + srvOffset);

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count   = m_4xMsaaState ? 4 : 1;
    sampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;

    CommandContext context(m_CommandListManager, D3D12_COMMAND_LIST_TYPE_DIRECT);
    return TextureBuffer(context, handle, sampleDesc, image);
}

void D3D12GraphicsManager::ClearShaders() { m_GraphicsPSO.clear(); }

void D3D12GraphicsManager::ClearBuffers() {
    m_CommandListManager.IdleGPU();
    m_DrawItems.clear();
    m_Meshes.clear();
    m_Buffers.clear();
    m_Textures.clear();
    ClearDebugBuffers();
    m_DebugMeshBuffer.clear();
}

void D3D12GraphicsManager::UpdateConstants() {
    GraphicsManager::UpdateConstants();
    auto currFR = m_FrameResource[m_CurrFrameResourceIndex].get();

    // Wait untill the frame is finished.
    if (!m_CommandListManager.GetGraphicsQueue().IsFenceComplete(currFR->fence)) {
        m_CommandListManager.WaitForFence(currFR->fence);
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
                oc.specularColor    = specularColor.ValueMap ? vec4f(-1.0f) : specularColor.Value;
                oc.specularPower    = material->GetSpecularPower().Value;
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

    CommandContext context(m_CommandListManager, D3D12_COMMAND_LIST_TYPE_DIRECT);
    PopulateCommandList(context);

    currFR->fence = context.Finish();
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % m_FrameCount;
}

void D3D12GraphicsManager::PopulateCommandList(CommandContext& context) {
    auto fr          = m_FrameResource[m_CurrFrameResourceIndex].get();
    auto commandList = context.GetCommandList();

    // change state from presentation to waitting to render.
    // TODO change commandList to context
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_CurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    context.SetViewportAndScissor(m_Viewport, m_ScissorRect);

    // clear buffer view
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    commandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0,
                                       0, nullptr);

    // render back buffer
    context.SetRenderTarget(CurrentBackBufferView(), DepthStencilView());

    context.SetRootSignature(m_RootSignature);
    context.SetDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 3, 0, m_SamplerDescriptors.GetDescriptorHandle());

    context.SetDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 0, 0,
                                 m_CbvSrvDescriptors.GetDescriptorHandle(m_FrameCBOffset + m_CurrFrameResourceIndex));

    DrawRenderItems(context, m_DrawItems);

#if defined(_DEBUG)
    DrawRenderItems(context, m_DebugDrawItems);
#endif  // DEBUG

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_CurrBackBuffer].Get(),
                                                   D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);
}

void D3D12GraphicsManager::DrawRenderItems(CommandContext& context, const std::vector<DrawItem>& drawItems) {
    //
    auto numObj = m_FrameResource[m_CurrFrameResourceIndex].get()->m_NumObjects;

    for (auto&& d : drawItems) {
        const auto& meshBuffer = d.meshBuffer;
        context.SetPipeLineState(m_GraphicsPSO[meshBuffer->psoName]);

        for (size_t i = 0; i < meshBuffer->vbv.size(); i++) {
            context.SetVertexBuffer(i, meshBuffer->vbv[i]);
        }
        context.SetIndexBuffer(meshBuffer->ibv);

        context.SetDynamicDescriptor(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, 0,
            m_CbvSrvDescriptors.GetDescriptorHandle(numObj * m_CurrFrameResourceIndex + d.constantBufferIndex));

        // Texture
        if (auto material = meshBuffer->material.lock()) {
            if (auto& pTexture = material->GetBaseColor().ValueMap) {
                context.SetDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, 0,
                                             m_Textures.at(pTexture->GetGuid()).GetSRV());
            }
        }
        context.SetPrimitiveTopology(meshBuffer->primitiveType);
        context.DrawIndexedInstanced(meshBuffer->indexCount, 1, 0, 0, 0);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::CurrentBackBufferView() const {
    return m_RtvDescriptors.GetDescriptorHandle(m_CurrBackBuffer);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::DepthStencilView() const {
    return m_DsvDescriptors.GetDescriptorHandle();
}

#if defined(_DEBUG)
void D3D12GraphicsManager::RenderLine(const vec3f& from, const vec3f& to, const vec3f& color) {
    std::string name;
    if (color.r > 0)
        name = "debug_line-x";
    else if (color.b > 0)
        name = "debug_line-y";
    else
        name = "debug_line-z";

    if (m_DebugMeshBuffer.find(name) == m_DebugMeshBuffer.end()) {
        auto meshBuffer                           = std::make_shared<MeshInfo>();
        meshBuffer->primitiveType                 = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        std::vector<vec3f>               position = {{0, 0, 0}, {1, 0, 0}};
        std::vector<int>                 index    = {0, 1};
        std::vector<vec3f>               colors(position.size(), color);
        Resource::SceneObjectVertexArray pos_array("POSITION", 0, Resource::VertexDataType::FLOAT3, position.data(),
                                                   position.size());
        Resource::SceneObjectVertexArray color_array("COLOR", 0, Resource::VertexDataType::FLOAT3, colors.data(),
                                                     colors.size());
        CreateVertexBuffer(pos_array, meshBuffer);
        CreateVertexBuffer(color_array, meshBuffer);
        Resource::SceneObjectIndexArray index_array(0, Resource::IndexDataType::INT32, index.data(), index.size());
        CreateIndexBuffer(index_array, meshBuffer);
        meshBuffer->indexCount  = index.size();
        meshBuffer->psoName     = "debug";
        m_DebugMeshBuffer[name] = meshBuffer;
    }

    vec3f v1          = to - from;
    vec3f v2          = {Length(v1), 0, 0};
    vec3f rotate_axis = v1 + v2;
    mat4f transform   = scale(mat4f(1.0f), vec3f(Length(v1)));
    transform         = rotate(transform, radians(180.0f), rotate_axis);
    transform         = translate(transform, from);

    auto node = std::make_shared<Resource::SceneGeometryNode>();
    node->AppendTransform(std::make_shared<Resource::SceneObjectTransform>(transform));
    m_DebugNode.push_back(node);

    DrawItem d;
    d.node                = node;
    d.meshBuffer          = m_DebugMeshBuffer[name];
    d.constantBufferIndex = m_DrawItems.size() + m_DebugDrawItems.size();
    d.numFramesDirty      = m_FrameResourceSize;
    m_DebugDrawItems.push_back(d);
}

void D3D12GraphicsManager::RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) {
    if (m_DebugMeshBuffer.find("debug_box") == m_DebugMeshBuffer.end()) {
        auto               meshBuffer = std::make_shared<MeshInfo>();
        std::vector<vec3f> position   = {
            {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1},
        };
        std::vector<int> index    = {0, 1, 2, 3, 0, 4, 5, 1, 5, 6, 2, 6, 7, 3, 7, 4};
        meshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        std::vector<vec3f>               colors(position.size(), color);
        Resource::SceneObjectVertexArray pos_array("POSITION", 0, Resource::VertexDataType::FLOAT3, position.data(),
                                                   position.size());
        Resource::SceneObjectVertexArray color_array("COLOR", 0, Resource::VertexDataType::FLOAT3, colors.data(),
                                                     colors.size());
        CreateVertexBuffer(pos_array, meshBuffer);
        CreateVertexBuffer(color_array, meshBuffer);
        Resource::SceneObjectIndexArray index_array(0, Resource::IndexDataType::INT32, index.data(), index.size());
        CreateIndexBuffer(index_array, meshBuffer);
        meshBuffer->indexCount = index.size();

        meshBuffer->psoName            = "debug";
        m_DebugMeshBuffer["debug_box"] = meshBuffer;
    }
    mat4f transform = translate(scale(mat4f(1.0f), 0.5 * (bbMax - bbMin)), 0.5 * (bbMax + bbMin));

    auto node = std::make_shared<Resource::SceneGeometryNode>();
    node->AppendTransform(std::make_shared<Resource::SceneObjectTransform>(transform));
    m_DebugNode.push_back(node);

    DrawItem d;
    d.node                = node;
    d.meshBuffer          = m_DebugMeshBuffer["debug_box"];
    d.constantBufferIndex = m_DrawItems.size() + m_DebugDrawItems.size();
    d.numFramesDirty      = m_FrameResourceSize;
    m_DebugDrawItems.push_back(d);
}

void D3D12GraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {}

void D3D12GraphicsManager::ClearDebugBuffers() {
    m_DebugNode.clear();
    m_DebugDrawItems.clear();
}

#endif  // DEBUG

}  // namespace Hitagi::Graphics::DX12