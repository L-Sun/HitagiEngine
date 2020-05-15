#include "D3D12GraphicsManager.hpp"

#include <spdlog/spdlog.h>

#include "D3DCore.hpp"
#include "GLFWApplication.hpp"
#include "IPhysicsManager.hpp"
#include "SceneManager.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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
    // Config
    auto config = g_App->GetConfiguration();
    auto window = static_cast<GLFWApplication*>(g_App.get())->GetWindow();
    auto hWnd   = glfwGetWin32Window(window);

    unsigned                              dxgiFactoryFlags = 0;
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;

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
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_Device)))) {
            Microsoft::WRL::ComPtr<IDXGIAdapter4> pWarpaAdapter;
            ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpaAdapter)));
            ThrowIfFailed(D3D12CreateDevice(pWarpaAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_Device)));
        }
    }

    // Detect 4x MSAA
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Flags            = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.Format           = m_BackBufferFormat;
        msQualityLevels.NumQualityLevels = 0;
        msQualityLevels.SampleCount      = 4;
        ThrowIfFailed(g_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels,
                                                    sizeof(msQualityLevels)));

        m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
        assert(m_4xMsaaQuality > 0 && "Unexpected MSAA qulity level.");
    }
    // Create command queue, allocator and list.
    g_CommandManager.Initialize(g_Device.Get());

    // Create Swap Chain
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount           = m_FrameCount;
        swapChainDesc.Width                 = g_App->GetConfiguration().screenWidth;
        swapChainDesc.Height                = g_App->GetConfiguration().screenHeight;
        swapChainDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.SampleDesc.Count      = 1;
        swapChainDesc.SampleDesc.Quality    = 0;
        swapChainDesc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(g_CommandManager.GetCommandQueue(), hWnd, &swapChainDesc,
                                                          nullptr, nullptr, &swapChain));

        ThrowIfFailed(swapChain.As(&m_SwapChain));
        ThrowIfFailed(dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
    }

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

void D3D12GraphicsManager::CreateDescriptorHeaps() {
    // Create rtv.
    m_RtvDescriptors = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(m_FrameCount);
    for (unsigned i = 0; i < m_FrameCount; i++) {
        ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
        g_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, m_RtvDescriptors.GetDescriptorCpuHandle(i));
    }

    // Create dsv.
    m_DsvDescriptors = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate();
    auto config      = g_App->GetConfiguration();

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
    ThrowIfFailed(g_Device->CreateCommittedResource(&heap_properties,
                                                    D3D12_HEAP_FLAG_NONE,
                                                    &dsvDesc,
                                                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                    &optClear,
                                                    IID_PPV_ARGS(&m_DepthStencilBuffer)));

    g_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, DepthStencilView());

    D3D12_RESOURCE_DESC rtDesc = {};
    rtDesc.DepthOrArraySize    = 1;
    rtDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rtDesc.Format              = m_BackBufferFormat;
    rtDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    rtDesc.Width               = config.screenWidth;
    rtDesc.Height              = config.screenHeight;
    rtDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rtDesc.MipLevels           = 1;
    rtDesc.SampleDesc.Count    = 1;
    ThrowIfFailed(g_Device->CreateCommittedResource(&heap_properties,
                                                    D3D12_HEAP_FLAG_NONE,
                                                    &rtDesc,
                                                    D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                    nullptr,
                                                    IID_PPV_ARGS(&m_RaytracingOutput)));

    // Create CBV SRV UAV Descriptor Heap
    size_t CbvSrvUavDescriptorsCount = m_FrameResourceSize * (m_MaxObjects + 1) + m_MaxTextures;
    m_CbvSrvUavDescriptors           = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(CbvSrvUavDescriptorsCount);

    // Use the last n descriptor as the frame constant buffer descriptor
    m_FrameCBOffset = m_FrameResourceSize * m_MaxObjects;
    m_SrvOffset     = m_FrameResourceSize * (m_MaxObjects + 1);

    m_SamplerDescriptors = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate();

    size_t raytracingDescriptorCount =
        1 +                         // BVH SRV Descriptor
        1 +                         // Output UAV Descriptor
        CbvSrvUavDescriptorsCount;  // Orther
    m_RaytracingDescriptorHeap.Initalize(L"Raytracing CbvSrvUav Descriptor Heap",
                                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                         raytracingDescriptorCount);

    m_RaytracingBVHDescriptor     = m_RaytracingDescriptorHeap.Allocate(1);
    m_RaytracingOutPutDescriptor  = m_RaytracingDescriptorHeap.Allocate(1);
    m_RaytracingCbvSrvDescriptors = m_RaytracingDescriptorHeap.Allocate(CbvSrvUavDescriptorsCount);
}

void D3D12GraphicsManager::CreateFrameResource() {
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        m_FrameResource.push_back(std::make_unique<FR>(m_MaxObjects));
    }
}

void D3D12GraphicsManager::CreateRootSignature() {
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(g_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    m_RootSignature.Reset(4, 0);
    m_RootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, 0);
    m_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0);
    m_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    m_RootSignature.Finalize(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                             featureData.HighestVersion);

    // DXR Root Signature
    // ---------------------------------------------------
    // Description      |       Type       | reg.(sp.) | belong to
    // ----------------------------------------------------
    // BVH              | Descriptor Table |   t0 (0)  | global g0
    // Output           | Descriptor Table |   u0 (0)  | local (raygen)
    // Frame constant   | Descriptor Table |   b0 (0)  | glabal g1
    // Random Numbers   | Root Descriptor  |   t1 (0)  | glabal g2
    // Object constant  | Descriptor Table |   b0 (1)  | local (hit group)
    // Object Normal    | Root Descriptor  |   t0 (1)  | local (hit group)
    // Object Index     | Root Descriptor  |   t1 (1)  | local (hit group)
    // ----------------------------------------------------

    m_RayTracingGlobalRootSignature.Reset(3, 0);
    m_RayTracingGlobalRootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, 0);  // t0
    m_RayTracingGlobalRootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, 0);  // b0
    m_RayTracingGlobalRootSignature[2].InitAsShaderResourceView(1, 0);                                   // b0
    m_RayTracingGlobalRootSignature.Finalize(D3D12_ROOT_SIGNATURE_FLAG_NONE, featureData.HighestVersion);

    m_RayGenSignature.Reset(1, 0);
    m_RayGenSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);  // u0
    m_RayGenSignature.Finalize(D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, featureData.HighestVersion);

    m_HitSignature.Reset(3, 0);
    m_HitSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, 1);  // b0
    m_HitSignature[1].InitAsShaderResourceView(0, 1);                                   // t0
    m_HitSignature[2].InitAsShaderResourceView(1, 1);                                   // t1
    m_HitSignature.Finalize(D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, featureData.HighestVersion);

    m_MissSignature.Finalize(D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, featureData.HighestVersion);
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
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = fr->m_ObjectConstantBuffer.GpuVirtualAddress;

        for (size_t j = 0; j < fr->m_NumObjects; j++) {
            cbDsec.BufferLocation = cbAddress;
            g_Device->CreateConstantBufferView(&cbDsec, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(offset));
            g_Device->CreateConstantBufferView(&cbDsec, m_RaytracingCbvSrvDescriptors.GetDescriptorCpuHandle(offset));

            cbAddress += cbDsec.SizeInBytes;
            offset++;
        }
    }
    // Create frame constant buffer, handle have been at the begin of
    // last m descriptor area
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        auto                            fr = m_FrameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc;
        cbDesc.SizeInBytes    = fr->FrameConstantSize();
        cbDesc.BufferLocation = fr->m_FrameConstantBuffer.GpuVirtualAddress;
        g_Device->CreateConstantBufferView(&cbDesc, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(offset));
        g_Device->CreateConstantBufferView(&cbDesc, m_RaytracingCbvSrvDescriptors.GetDescriptorCpuHandle(offset));
        offset++;
    }

    {  // bind raytracing output descriptor
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;
        g_Device->CreateUnorderedAccessView(m_RaytracingOutput.Get(),
                                            nullptr,
                                            &uavDesc,
                                            m_RaytracingOutPutDescriptor.GetDescriptorCpuHandle());
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

    g_Device->CreateSampler(&samplerDesc, m_SamplerDescriptors.GetDescriptorCpuHandle());
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
    m_GraphicsPSO["no_texture"].Finalize();

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
    m_GraphicsPSO["texture"].Finalize();

    auto                        buffer = g_FileIOManager->SyncOpenAndReadBinary("Asset/Shaders/Raytracing.cso");
    RaytracingPipelineGenerator rsPsoGenerator;
    rsPsoGenerator.AddLibrary(buffer, {L"RayGen", L"Miss", L"ClosestHit"});
    rsPsoGenerator.AddHitGroup(L"HitGroup", L"ClosestHit");
    rsPsoGenerator.SetGlobalSignature(m_RayTracingGlobalRootSignature);
    rsPsoGenerator.AddRootSignatureAssociation(m_RayGenSignature, {L"RayGen"});
    rsPsoGenerator.AddRootSignatureAssociation(m_MissSignature, {L"Miss"});
    rsPsoGenerator.AddRootSignatureAssociation(m_HitSignature, {L"HitGroup"});

    struct payload {
        vec4f color;
        int   depth;
        float testT;
        float T;
        int   isEmission;
    };
    rsPsoGenerator.SetMaxPayloadSize(sizeof(payload));
    rsPsoGenerator.SetMaxAttributeSize(2 * sizeof(float));
    rsPsoGenerator.SetMaxRecursionDepth(16);

    m_RaytracingPSO = rsPsoGenerator.Generate();
    ThrowIfFailed(m_RaytracingPSO->QueryInterface(IID_PPV_ARGS(&m_RaytracingStateObjectProps)));

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
    m_GraphicsPSO["debug"].Finalize();
#endif  // DEBUG
}

void D3D12GraphicsManager::InitializeBuffers(const Resource::Scene& scene) {
    // Initialize meshes buffer
    std::wstring       name;
    std::wstringstream wss;

    for (auto&& [key, geometry] : scene.Geometries) {
        for (auto&& mesh : geometry->GetMeshes()) {
            auto m = std::make_shared<MeshInfo>();
            for (auto&& inputLayout : m_InputLayout) {
                if (mesh->HasProperty(inputLayout.SemanticName)) {
                    auto& vertexArray = mesh->GetVertexPropertyArray(inputLayout.SemanticName);
                    wss << inputLayout.SemanticName;
                    wss >> name;
                    m->verticesBuffer.emplace_back(name, vertexArray.GetVertexCount(),
                                                   vertexArray.GetDataSize() / vertexArray.GetVertexCount(),
                                                   vertexArray.GetData());
                }
            }
            auto& indexArray = mesh->GetIndexArray();
            m->indexCount    = indexArray.GetIndexCount();
            m->indicesBuffer = GpuBuffer(L"Index Buffer", indexArray.GetIndexCount(),
                                         indexArray.GetDataSize() / indexArray.GetIndexCount(), indexArray.GetData());
            SetPrimitiveType(mesh->GetPrimitiveType(), m);
            m_Meshes[mesh->GetGuid()] = m;
        }
    }
    // Intialize textures buffer
    size_t i = 0;
    for (auto&& [key, material] : scene.Materials) {
        if (material) {
            if (auto texture = material->GetDiffuseColor().ValueMap; texture != nullptr) {
                auto  guid  = texture->GetGuid();
                auto& image = texture->GetTextureImage();
                m_Textures.insert({guid, CreateTextureBuffer(image, i)});
                i++;
            }
        }
    }
    // Initialize draw item
    for (auto&& [key, node] : scene.GeometryNodes) {
        if (node && node->Visible()) {
            if (auto geometry = node->GetSceneObjectRef().lock()) {
                for (auto&& mesh : geometry->GetMeshes()) {
                    DrawItem d;
                    d.constantBufferIndex = m_DrawItems.size();
                    d.meshBuffer          = m_Meshes.at(mesh->GetGuid());
                    d.node                = node;
                    d.numFramesDirty      = m_FrameResourceSize;
                    if (auto m = mesh->GetMaterial().lock()) {
                        d.material = m;
                        d.psoName  = m->GetDiffuseColor().ValueMap ? "texture" : "no_texture";
                    } else {
                        d.psoName = "simple";
                    }
                    m_DrawItems.push_back(std::move(d));
                }
            }
        }
    }

    // Initailize Acceleration structure
    CommandContext         context("Ray Tracing");
    BottomLevelASGenerator BLASGenerator;
    TopLevelASGenerator    TLASGenerator;
    size_t                 id = 0;
    m_BottomLevelAS.resize(m_DrawItems.size());
    for (size_t i = 0; i < m_DrawItems.size(); i++) {
        BLASGenerator.AddMesh(*m_DrawItems[i].meshBuffer);
        m_BottomLevelAS[i] = BLASGenerator.Generate(context, false, nullptr);
        TLASGenerator.AddInstance(m_BottomLevelAS[i], m_DrawItems[i].node.lock()->GetCalculatedTransform(), id, id);
        context.Finish(true);
        context.Reset();
        BLASGenerator.Reset();
        id++;
    }
    m_TopLevelAS = TLASGenerator.Generate(context, false, nullptr);
    context.Finish(true);

    // bind the TLAS to srv descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc          = {};
    srvDesc.Format                                   = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_TopLevelAS->GetGPUVirtualAddress();
    g_Device->CreateShaderResourceView(nullptr, &srvDesc, m_RaytracingBVHDescriptor.GetDescriptorCpuHandle());

    // Create shader binding table
    {
        m_RayGenShaderTable = ShaderTable(L"RayGenTable");
        m_RayGenShaderTable.AddShaderRecord(
            m_RaytracingStateObjectProps->GetShaderIdentifier(L"RayGen"),
            m_RaytracingOutPutDescriptor.GetDescriptorGpuHandle());
        m_RayGenShaderTable.Generate();
    }
    {
        m_MissShaderTable = ShaderTable(L"MissTable");
        m_MissShaderTable.AddShaderRecord(
            m_RaytracingStateObjectProps->GetShaderIdentifier(L"Miss"));
        m_MissShaderTable.Generate();
    }
    {
        m_HitGroupShaderTable.resize(m_FrameResourceSize, ShaderTable(L"HitGroupShaderTable"));
        size_t frameIndex = 0;
        for (auto&& shaderTable : m_HitGroupShaderTable) {
            for (auto&& d : m_DrawItems) {
                shaderTable.AddShaderRecord(
                    m_RaytracingStateObjectProps->GetShaderIdentifier(L"HitGroup"),
                    m_RaytracingCbvSrvDescriptors.GetDescriptorGpuHandle(frameIndex * m_MaxObjects + d.constantBufferIndex),
                    d.meshBuffer->verticesBuffer[1].GetResource()->GetGPUVirtualAddress(),
                    d.meshBuffer->indicesBuffer.GetResource()->GetGPUVirtualAddress());
            }
            shaderTable.Generate();
            frameIndex++;
        }
    }

    // Generate random numbers for raytracing
    {
        std::mt19937                          rnd(std::random_device{}());
        std::uniform_real_distribution<float> gen(0.0f, 1.0f);
        std::vector<float>                    randomNumbers(10000);
        for (auto&& n : randomNumbers) n = gen(rnd);
        m_RandomNumbers = GpuBuffer(L"Random Numbers", randomNumbers.size(), sizeof(float), randomNumbers.data());
    }
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
    auto handle = m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(m_SrvOffset + srvOffset);

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count   = m_4xMsaaState ? 4 : 1;
    sampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;

    return TextureBuffer(handle, sampleDesc, image);
}

void D3D12GraphicsManager::ClearShaders() { m_GraphicsPSO.clear(); }

void D3D12GraphicsManager::ClearBuffers() {
    g_CommandManager.IdleGPU();
    m_DrawItems.clear();
    m_Meshes.clear();
    m_Textures.clear();

#if defined(_DEBUG)
    ClearDebugBuffers();
    m_DebugMeshBuffer.clear();
#endif  // DEBUG
}

void D3D12GraphicsManager::UpdateConstants() {
    GraphicsManager::UpdateConstants();

    auto currFR = m_FrameResource[m_CurrFrameResourceIndex].get();
    // Wait untill the frame is finished.
    if (!g_CommandManager.GetGraphicsQueue().IsFenceComplete(currFR->fence)) {
        g_CommandManager.WaitForFence(currFR->fence);
    }

    // Update frame resource
    FrameConstants fcb = m_FrameConstants;
    fcb.projView       = transpose(m_FrameConstants.projView);
    fcb.projection     = transpose(m_FrameConstants.projection);
    fcb.view           = transpose(m_FrameConstants.view);
    fcb.invView        = transpose(m_FrameConstants.invView);
    fcb.invProjection  = transpose(m_FrameConstants.invProjection);
    fcb.invProjView    = transpose(m_FrameConstants.invProjView);

    currFR->UpdateFrameConstants(fcb);
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
            oc.model = transpose(node->GetCalculatedTransform());

            if (auto material = d.material.lock()) {
                auto& ambientColor  = material->GetAmbientColor();
                oc.ambient          = ambientColor.ValueMap ? vec4f(-1.0f) : ambientColor.Value;
                auto& diffuseColor  = material->GetDiffuseColor();
                oc.diffuse          = diffuseColor.ValueMap ? vec4f(-1.0f) : diffuseColor.Value;
                auto& emissionColor = material->GetEmission();
                oc.emission         = emissionColor.ValueMap ? vec4f(-1.0f) : emissionColor.Value;
                auto& specularColor = material->GetSpecularColor();
                oc.specular         = specularColor.ValueMap ? vec4f(-1.0f) : specularColor.Value;
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
            oc.model = transpose(node->GetCalculatedTransform());
            currFR->UpdateObjectConstants(d.constantBufferIndex, oc);
        }
    }
#endif
}

void D3D12GraphicsManager::RenderBuffers() {
    auto currFR = m_FrameResource[m_CurrFrameResourceIndex].get();

    CommandContext context("Render");
    PopulateCommandList(context);

    currFR->fence = context.Finish();
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % m_FrameCount;
}

void D3D12GraphicsManager::PopulateCommandList(CommandContext& context) {
    auto fr          = m_FrameResource[m_CurrFrameResourceIndex].get();
    auto commandList = context.GetCommandList();

    context.SetRootSignature(m_RootSignature);
    context.SetViewportAndScissor(m_Viewport, m_ScissorRect);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_CurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    // clear buffer view

    // render back buffer
    context.SetRenderTarget(CurrentBackBufferView(), DepthStencilView());

    commandList->ClearDepthStencilView(DepthStencilView(),
                                       D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                       1.0f, 0,
                                       0, nullptr);
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    commandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor, 0, nullptr);

    context.SetDynamicSampler(3, 0, m_SamplerDescriptors.GetDescriptorCpuHandle());
    context.SetDynamicDescriptor(0, 0, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(m_FrameCBOffset + m_CurrFrameResourceIndex));

    if (m_Raster) {
        DrawRenderItems(context, m_DrawItems);
    } else {
        commandList->SetComputeRootSignature(m_RayTracingGlobalRootSignature.GetRootSignature());
        context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_RaytracingDescriptorHeap.GetHeapPointer());
        // BVH
        commandList->SetComputeRootDescriptorTable(0, m_RaytracingBVHDescriptor.GetDescriptorGpuHandle());
        // Frame constant buffer
        commandList->SetComputeRootDescriptorTable(1, m_RaytracingCbvSrvDescriptors.GetDescriptorGpuHandle(m_FrameCBOffset + m_CurrFrameResourceIndex));
        // Random Numbers
        commandList->SetComputeRootShaderResourceView(2, m_RandomNumbers.GetResource()->GetGPUVirtualAddress());

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RaytracingOutput.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &barrier);

        D3D12_DISPATCH_RAYS_DESC desc = {};

        desc.RayGenerationShaderRecord.StartAddress = m_RayGenShaderTable.GetGpuBuffer()->GetGPUVirtualAddress();
        desc.RayGenerationShaderRecord.SizeInBytes  = m_RayGenShaderTable.GetSize();

        desc.MissShaderTable.StartAddress  = m_MissShaderTable.GetGpuBuffer()->GetGPUVirtualAddress();
        desc.MissShaderTable.SizeInBytes   = m_MissShaderTable.GetSize();
        desc.MissShaderTable.StrideInBytes = m_MissShaderTable.GetStride();

        desc.HitGroupTable.StartAddress  = m_HitGroupShaderTable[m_CurrFrameResourceIndex].GetGpuBuffer()->GetGPUVirtualAddress();
        desc.HitGroupTable.SizeInBytes   = m_HitGroupShaderTable[m_CurrFrameResourceIndex].GetSize();
        desc.HitGroupTable.StrideInBytes = m_HitGroupShaderTable[m_CurrFrameResourceIndex].GetStride();

        desc.Width  = m_Viewport.Width;
        desc.Height = m_Viewport.Height;
        desc.Depth  = 1;
        commandList->SetPipelineState1(m_RaytracingPSO.Get());
        commandList->DispatchRays(&desc);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RaytracingOutput.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RenderTargets[m_CurrBackBuffer].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &barrier);

        commandList->CopyResource(m_RenderTargets[m_CurrBackBuffer].Get(),
                                  m_RaytracingOutput.Get());

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RenderTargets[m_CurrBackBuffer].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);
    }

#if defined(_DEBUG)
    DrawRenderItems(context, m_DebugDrawItems);
#endif  // DEBUG

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_CurrBackBuffer].Get(),
                                                   D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);
}

void D3D12GraphicsManager::DrawRenderItems(CommandContext& context, const std::vector<DrawItem>& drawItems) {
    auto numObj = m_FrameResource[m_CurrFrameResourceIndex].get()->m_NumObjects;
    for (auto&& d : drawItems) {
        const auto& meshBuffer = d.meshBuffer;
        context.SetPipeLineState(m_GraphicsPSO.at(d.psoName));

        for (size_t i = 0; i < meshBuffer->verticesBuffer.size(); i++) {
            context.SetVertexBuffer(i, meshBuffer->verticesBuffer[i].VertexBufferView());
        }
        context.SetIndexBuffer(meshBuffer->indicesBuffer.IndexBufferView());

        context.SetDynamicDescriptor(1, 0, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(numObj * m_CurrFrameResourceIndex + d.constantBufferIndex));

        // Texture
        if (auto material = d.material.lock()) {
            if (auto& pTexture = material->GetDiffuseColor().ValueMap) {
                context.SetDynamicDescriptor(2, 0, m_Textures.at(pTexture->GetGuid()).GetSRV());
            }
        }
        context.SetPrimitiveTopology(meshBuffer->primitiveType);
        context.DrawIndexedInstanced(meshBuffer->indexCount, 1, 0, 0, 0);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::CurrentBackBufferView() const {
    return m_RtvDescriptors.GetDescriptorCpuHandle(m_CurrBackBuffer);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsManager::DepthStencilView() const {
    return m_DsvDescriptors.GetDescriptorCpuHandle();
}

void D3D12GraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {}

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
        auto meshBuffer             = std::make_shared<MeshInfo>();
        meshBuffer->primitiveType   = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        std::vector<vec3f> position = {{0, 0, 0}, {1, 0, 0}};
        std::vector<int>   index    = {0, 1};
        std::vector<vec3f> colors(position.size(), color);

        meshBuffer->verticesBuffer.emplace_back(L"Debug Position", position.size(), sizeof(vec3f), position.data());
        meshBuffer->verticesBuffer.emplace_back(L"Debug Color", colors.size(), sizeof(vec3f), colors.data());
        meshBuffer->indicesBuffer = GpuBuffer(L"Debug Index Buffer", index.size(), sizeof(int), index.data());
        meshBuffer->indexCount    = index.size();
        m_DebugMeshBuffer[name]   = meshBuffer;
    }

    vec3f v1          = to - from;
    vec3f v2          = {v1.norm(), 0, 0};
    vec3f rotate_axis = v1 + v2;
    mat4f transform   = scale(mat4f(1.0f), vec3f(v1.norm()));
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
    d.psoName             = "debug";
    m_DebugDrawItems.push_back(d);
}

void D3D12GraphicsManager::RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) {
    if (m_DebugMeshBuffer.find("debug_box") == m_DebugMeshBuffer.end()) {
        auto               meshBuffer = std::make_shared<MeshInfo>();
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
        std::vector<int> index    = {0, 1, 2, 3, 0, 4, 5, 1, 5, 6, 2, 6, 7, 3, 7, 4};
        meshBuffer->primitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        std::vector<vec3f> colors(position.size(), color);
        meshBuffer->verticesBuffer.emplace_back(L"Debug Position", position.size(), sizeof(vec3f), position.data());
        meshBuffer->verticesBuffer.emplace_back(L"Debug Color", colors.size(), sizeof(vec3f), colors.data());
        meshBuffer->indicesBuffer = GpuBuffer(L"Debug Index Buffer", index.size(), sizeof(int), index.data());
        meshBuffer->indexCount    = index.size();

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
    d.psoName             = "debug";
    m_DebugDrawItems.push_back(d);
}

void D3D12GraphicsManager::ClearDebugBuffers() {
    m_DebugNode.clear();
    m_DebugDrawItems.clear();
}

#endif  // DEBUG

}  // namespace Hitagi::Graphics::DX12