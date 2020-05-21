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
    for (unsigned i = 0; i < m_FrameCount; i++) {
        Microsoft::WRL::ComPtr<ID3D12Resource> displayPlane;
        ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&displayPlane)));
        m_DisplayPlanes[i].CreateFromSwapChain(L"Primary Display Plane", displayPlane.Detach());
        m_DisplayPlanes[i].SetClearColor(vec4f{0.1f, 0.1f, 0.1f, 1.0f});
    }

    // Create dsv.
    auto config = g_App->GetConfiguration();
    m_SceneDepthBuffer.Create(L"Scene Depth Buffer", config.screenWidth, config.screenHeight, m_DepthStencilFormat);

    // Create CBV SRV UAV Descriptor Heap
    size_t CbvSrvUavDescriptorsCount = m_FrameResourceSize * (m_MaxObjects + 1) + m_MaxTextures;
    m_CbvSrvUavDescriptors           = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(CbvSrvUavDescriptorsCount);

    // Use the last n descriptor as the frame constant buffer descriptor
    m_FrameCBOffset = m_FrameResourceSize * m_MaxObjects;
    m_SrvOffset     = m_FrameResourceSize * (m_MaxObjects + 1);

    m_SamplerDescriptors = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate();
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
            g_Device->CreateConstantBufferView(&cbDsec, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(offset++));
            cbAddress += cbDsec.SizeInBytes;
        }
    }
    // Create frame constant buffer, handle have been at the begin of
    // last m descriptor area
    for (size_t i = 0; i < m_FrameResourceSize; i++) {
        auto                            fr = m_FrameResource[i].get();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc;
        cbDesc.SizeInBytes    = fr->FrameConstantSize();
        cbDesc.BufferLocation = fr->m_FrameConstantBuffer.GpuVirtualAddress;
        g_Device->CreateConstantBufferView(&cbDesc, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(offset++));
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

    m_DebugInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0}};

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
    m_GraphicsPSO["no_texture"].SetRenderTargetFormats({m_BackBufferFormat}, m_DepthStencilFormat);
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
    m_GraphicsPSO["texture"].SetRenderTargetFormats({m_BackBufferFormat}, m_DepthStencilFormat);
    m_GraphicsPSO["texture"].Finalize();

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
    m_GraphicsPSO["debug"].SetRenderTargetFormats({m_BackBufferFormat}, m_DepthStencilFormat);
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
                    m->verticesBuffer.emplace_back(name, vertexArray.GetVertexCount(), vertexArray.GetVertexSize(), vertexArray.GetData());
                }
            }
            auto& indexArray = mesh->GetIndexArray();
            m->indexCount    = indexArray.GetIndexCount();
            m->indicesBuffer.Create(L"Index Buffer", indexArray.GetIndexCount(), indexArray.GetIndexSize(), indexArray.GetData());
            m->primitiveType          = GetD3DPrimitiveType(mesh->GetPrimitiveType());
            m_Meshes[mesh->GetGuid()] = m;
        }
    }
    // Intialize textures buffer
    size_t i = 0;
    for (auto&& [key, material] : scene.Materials) {
        if (material) {
            if (auto texture = material->GetDiffuseColor().ValueMap; texture != nullptr) {
                auto               guid  = texture->GetGuid();
                auto&              image = texture->GetTextureImage();
                const std::string& name  = texture->GetName();
                m_Textures.insert({guid, TextureBuffer(std::wstring(name.begin(), name.end()), image)});
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
}

D3D_PRIMITIVE_TOPOLOGY D3D12GraphicsManager::GetD3DPrimitiveType(const Resource::PrimitiveType& primitiveType) {
    switch (primitiveType) {
        case Resource::PrimitiveType::LINE_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case Resource::PrimitiveType::LINE_LIST_ADJACENCY:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
        case Resource::PrimitiveType::LINE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case Resource::PrimitiveType::LINE_STRIP_ADJACENCY:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
        case Resource::PrimitiveType::TRI_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case Resource::PrimitiveType::TRI_LIST_ADJACENCY:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case Resource::PrimitiveType::POINT_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

void D3D12GraphicsManager::ClearShaders() { m_GraphicsPSO.clear(); }

void D3D12GraphicsManager::ClearBuffers() {
    g_CommandManager.IdleGPU();
    m_DrawItems.clear();
    m_Meshes.clear();
    m_Textures.clear();

#if defined(_DEBUG)
    ClearDebugBuffers();
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
            d.constants.model = transpose(node->GetCalculatedTransform());

            if (auto material = d.material.lock()) {
                auto& ambientColor        = material->GetAmbientColor();
                d.constants.ambient       = ambientColor.ValueMap ? vec4f(-1.0f) : ambientColor.Value;
                auto& diffuseColor        = material->GetDiffuseColor();
                d.constants.diffuse       = diffuseColor.ValueMap ? vec4f(-1.0f) : diffuseColor.Value;
                auto& emissionColor       = material->GetEmission();
                d.constants.emission      = emissionColor.ValueMap ? vec4f(-1.0f) : emissionColor.Value;
                auto& specularColor       = material->GetSpecularColor();
                d.constants.specular      = specularColor.ValueMap ? vec4f(-1.0f) : specularColor.Value;
                d.constants.specularPower = material->GetSpecularPower().Value;
            }

            currFR->UpdateObjectConstants(d.constantBufferIndex, d.constants);
            d.numFramesDirty--;
        }
    }

#if defined(_DEBUG)
    for (auto&& d : m_DebugDrawItems) {
        if (auto node = d.node.lock()) {
            d.constants.model = transpose(node->GetCalculatedTransform());
            currFR->UpdateObjectConstants(d.constantBufferIndex, d.constants);
        }
    }
#endif
}

void D3D12GraphicsManager::RenderBuffers() {
    auto currFR = m_FrameResource[m_CurrFrameResourceIndex].get();

    CommandContext context("Render");
    PopulateCommandList(context);

    currFR->fence = context.Finish(true);
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % m_FrameCount;
}

void D3D12GraphicsManager::PopulateCommandList(CommandContext& context) {
    auto fr = m_FrameResource[m_CurrFrameResourceIndex].get();

    context.SetRootSignature(m_RootSignature);
    context.SetViewportAndScissor(m_Viewport, m_ScissorRect);
    context.TransitionResource(m_DisplayPlanes[m_CurrBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.TransitionResource(m_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

    // render back buffer
    context.SetRenderTarget(m_DisplayPlanes[m_CurrBackBuffer].GetRTV(), m_SceneDepthBuffer.GetDSV());

    context.ClearDepth(m_SceneDepthBuffer);
    context.ClearColor(m_DisplayPlanes[m_CurrBackBuffer]);

    context.SetDynamicSampler(3, 0, m_SamplerDescriptors.GetDescriptorCpuHandle());
    context.SetDynamicDescriptor(0, 0, m_CbvSrvUavDescriptors.GetDescriptorCpuHandle(m_FrameCBOffset + m_CurrFrameResourceIndex));

    DrawRenderItems(context, m_DrawItems);

#if defined(_DEBUG)
    DrawRenderItems(context, m_DebugDrawItems);
#endif  // DEBUG
    context.TransitionResource(m_DisplayPlanes[m_CurrBackBuffer], D3D12_RESOURCE_STATE_PRESENT);
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

void D3D12GraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {}

#if defined(_DEBUG)
void D3D12GraphicsManager::DrawDebugMesh(const Resource::SceneObjectMesh& mesh, const mat4f& transform, const vec4f& color) {
    DrawItem drawItem;
    // initialize mesh buffer
    drawItem.meshBuffer  = std::make_shared<MeshInfo>();
    const auto& position = mesh.GetVertexPropertyArray("POSITION");
    const auto& indices  = mesh.GetIndexArray();
    drawItem.meshBuffer->verticesBuffer.emplace_back(
        L"POSITION", position.GetVertexCount(), position.GetVertexSize(), position.GetData());
    drawItem.meshBuffer->indicesBuffer.Create(L"Index Buffer", indices.GetIndexCount(), indices.GetIndexSize(), indices.GetData());
    drawItem.meshBuffer->primitiveType = GetD3DPrimitiveType(mesh.GetPrimitiveType());
    drawItem.meshBuffer->indexCount    = indices.GetIndexCount();

    m_DebugNode.emplace_back(std::make_shared<Resource::SceneGeometryNode>());
    m_DebugNode.back()->ApplyTransform(transform);

    drawItem.node                = m_DebugNode.back();
    drawItem.constantBufferIndex = m_DrawItems.size() + m_DebugDrawItems.size();
    drawItem.numFramesDirty      = m_FrameResourceSize;
    drawItem.psoName             = "debug";
    drawItem.constants.diffuse   = color;

    m_DebugDrawItems.emplace_back(std::move(drawItem));
}

void D3D12GraphicsManager::ClearDebugBuffers() {
    m_DebugNode.clear();
    m_DebugDrawItems.clear();
}

#endif  // DEBUG

}  // namespace Hitagi::Graphics::DX12