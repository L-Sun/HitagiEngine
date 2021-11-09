#include "DX12DriverAPI.hpp"

#include "GpuBuffer.hpp"
#include "CommandContext.hpp"
#include "Sampler.hpp"

#include <windef.h>

namespace Hitagi::Graphics::backend::DX12 {
ComPtr<ID3D12DebugDevice1> DX12DriverAPI::sm_DebugInterface = nullptr;

DX12DriverAPI::DX12DriverAPI() : DriverAPI(APIType::DirectX12) {
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
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_DxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&m_Device)))) {
            ComPtr<IDXGIAdapter4> pWarpaAdapter;
            ThrowIfFailed(m_DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpaAdapter)));
            ThrowIfFailed(D3D12CreateDevice(pWarpaAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_Device)));
        }
    }

    // Initialize command manager
    m_CommandManager.Initialize(m_Device.Get());
    // Iniaizlie Descriptor Allocator
    for (auto&& allocator : m_DescriptorAllocator)
        allocator.Initialize(m_Device.Get());
}

DX12DriverAPI::~DX12DriverAPI() {
    m_CommandManager.IdleGPU();
    // Release the static variable that is allocation of DX12
    LinearAllocator::Destroy();
    DynamicDescriptorHeap::Destroy();
    ThrowIfFailed(m_Device->QueryInterface(sm_DebugInterface.ReleaseAndGetAddressOf()));
}

void DX12DriverAPI::ReportDebugLog() {
    sm_DebugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
}

void DX12DriverAPI::Present(size_t frameIndex) {
    ThrowIfFailed(m_SwapChain->Present(0, 0));
}

void DX12DriverAPI::CreateSwapChain(uint32_t width, uint32_t height, unsigned frameCount, Format format, void* window) {
    auto hWnd = *reinterpret_cast<HWND*>(window);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount           = frameCount;
    swapChainDesc.Width                 = width;
    swapChainDesc.Height                = height;
    swapChainDesc.Format                = ToDxgiFormat(format);
    swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count      = 1;
    swapChainDesc.SampleDesc.Quality    = 0;
    swapChainDesc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(m_DxgiFactory->CreateSwapChainForHwnd(m_CommandManager.GetCommandQueue(), hWnd, &swapChainDesc,
                                                        nullptr, nullptr, &swapChain));

    ThrowIfFailed(swapChain.As(&m_SwapChain));
    ThrowIfFailed(m_DxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
}

Graphics::Resource DX12DriverAPI::GetSwapChainBuffer(size_t frameIndex) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> _res;
    m_SwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&_res));
    return {"Swap chain buffer", std::make_unique<GpuResource>(_res.Detach())};
}

Graphics::RenderTarget DX12DriverAPI::CreateRenderTarget(std::string_view name, const Graphics::RenderTarget::Description& desc) {
    D3D12_RESOURCE_DESC _desc = {};
    _desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    _desc.SampleDesc.Count    = 1;
    _desc.SampleDesc.Quality  = 0;
    _desc.Width               = desc.width;
    _desc.Height              = desc.height;
    _desc.DepthOrArraySize    = 1;
    _desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    _desc.Format              = ToDxgiFormat(desc.format);

    return {name,
            std::make_unique<RenderTarget>(
                name,
                m_Device.Get(),
                m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(),
                _desc),
            desc};
}

Graphics::RenderTarget DX12DriverAPI::CreateRenderFromSwapChain(size_t frameIndex) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> res;
    m_SwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&res));
    auto rt = std::make_unique<RenderTarget>(
        "RT for SwapChain",
        m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(),
        res.Detach());
    auto desc = rt->GetResource()->GetDesc();
    return {"RT for SwapChain",
            std::move(rt),
            Graphics::RenderTarget::Description{
                .format = ToFormat(desc.Format),
                .width  = desc.Width,
                .height = desc.Height,
            }};
}

Graphics::VertexBuffer DX12DriverAPI::CreateVertexBuffer(std::string_view name, size_t vertexCount, size_t vertexSize, const uint8_t* initialData) {
    auto buffer = std::make_unique<VertexBuffer>(m_Device.Get(), "Vertex", vertexCount, vertexSize);
    if (initialData) {
        CopyCommandContext context(*this);
        context.InitializeBuffer(*buffer, initialData, buffer->GetBufferSize());
    }
    return {name, std::move(buffer)};
}

Graphics::IndexBuffer DX12DriverAPI::CreateIndexBuffer(std::string_view name, size_t indexCount, size_t indexSize, const uint8_t* initialData) {
    auto buffer = std::make_unique<IndexBuffer>(m_Device.Get(), "Index", indexCount, indexSize);
    if (initialData) {
        CopyCommandContext context(*this);
        context.InitializeBuffer(*buffer, initialData, buffer->GetBufferSize());
    }
    return {name, std::move(buffer)};
}

Graphics::ConstantBuffer DX12DriverAPI::CreateConstantBuffer(std::string_view name, size_t numElements, size_t elementSize) {
    return {
        name,
        std::make_unique<ConstantBuffer>(
            name,
            m_Device.Get(),
            m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
            numElements,
            elementSize),
        numElements,
        elementSize};
}

Graphics::TextureBuffer DX12DriverAPI::CreateTextureBuffer(std::string_view name, const Graphics::TextureBuffer::Description& desc) {
    D3D12_RESOURCE_DESC _desc = {};
    _desc.Width               = desc.width;
    _desc.Height              = desc.height;
    _desc.Format              = ToDxgiFormat(desc.format);
    _desc.MipLevels           = desc.mipLevel;
    _desc.DepthOrArraySize    = 1;
    _desc.SampleDesc.Count    = desc.sampleCount;
    _desc.SampleDesc.Quality  = desc.sampleQuality;
    _desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto buffer = std::make_unique<TextureBuffer>(
        name,
        m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(),
        _desc);
    if (desc.initialData) {
        CopyCommandContext     context(*this);
        D3D12_SUBRESOURCE_DATA subData;
        subData.pData      = desc.initialData;
        subData.RowPitch   = desc.pitch;
        subData.SlicePitch = desc.initialDataSize;
        context.InitializeTexture(*buffer, {subData});
    }
    return {name, std::move(buffer), desc};
}

Graphics::DepthBuffer DX12DriverAPI::CreateDepthBuffer(std::string_view name, const Graphics::DepthBuffer::Description& desc) {
    auto _desc  = CD3DX12_RESOURCE_DESC::Tex2D(ToDxgiFormat(desc.format), desc.width, desc.height);
    _desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    return {name,
            std::make_unique<DepthBuffer>(
                name,
                m_Device.Get(),
                m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate(),
                _desc,
                desc.clearDepth,
                desc.clearStencil),
            desc};
}

void DX12DriverAPI::UpdateConstantBuffer(Graphics::ConstantBuffer& buffer, size_t offset, const uint8_t* data, size_t size) {
    assert(data != nullptr);
    if (size % buffer.GetElementSize() != 0) {
        throw std::invalid_argument("the size of input data must be an integer multiple of element size!");
    }
    if (size > (buffer.GetNumElements() - offset) * buffer.GetElementSize()) {
        throw std::out_of_range("the input data is out of range of constant buffer!");
    }
    auto cb = static_cast<ConstantBuffer*>(buffer.GetGpuResource());
    cb->UpdateData(offset, data, size);
}

void DX12DriverAPI::RetireResources(std::vector<Graphics::Resource>&& resources, uint64_t fenceValue) {
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.front().first))
        m_RetireResources.pop();

    m_RetireResources.emplace(fenceValue, std::move(resources));
}

// TODO: Custom sampler
Graphics::TextureSampler DX12DriverAPI::CreateSampler(std::string_view name, const Graphics::TextureSampler::Description& desc) {
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.ComparisonFunc     = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.Filter             = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    samplerDesc.MaxAnisotropy      = 1;
    samplerDesc.MaxLOD             = D3D12_FLOAT32_MAX;
    samplerDesc.MinLOD             = 0;
    samplerDesc.MipLODBias         = 0.0f;

    return {name,
            std::make_unique<Sampler>(
                m_Device.Get(),
                m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate(),
                samplerDesc),
            desc};
}

void DX12DriverAPI::CreateRootSignature(const Graphics::RootSignature& signature) {
    auto& parameterTable = signature.GetParametes();
    if (parameterTable.empty()) return;

    constexpr auto numVisibility = static_cast<int>(Graphics::ShaderVisibility::Num_Visibility);
    constexpr auto numVarType    = static_cast<int>(Graphics::ShaderVariableType::Num_Type);
    static const std::unordered_map<Graphics::ShaderVisibility, D3D12_SHADER_VISIBILITY>
        visibilityCast = {
            {Graphics::ShaderVisibility::All, D3D12_SHADER_VISIBILITY_ALL},
            {Graphics::ShaderVisibility::Vertex, D3D12_SHADER_VISIBILITY_VERTEX},
            {Graphics::ShaderVisibility::Hull, D3D12_SHADER_VISIBILITY_HULL},
            {Graphics::ShaderVisibility::Domain, D3D12_SHADER_VISIBILITY_DOMAIN},
            {Graphics::ShaderVisibility::Geometry, D3D12_SHADER_VISIBILITY_GEOMETRY},
            {Graphics::ShaderVisibility::Pixel, D3D12_SHADER_VISIBILITY_PIXEL},
        };
    static const std::unordered_map<Graphics::ShaderVariableType, D3D12_DESCRIPTOR_RANGE_TYPE>
        typeCast = {
            {Graphics::ShaderVariableType::CBV, D3D12_DESCRIPTOR_RANGE_TYPE_CBV},
            {Graphics::ShaderVariableType::SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV},
            {Graphics::ShaderVariableType::UAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV},
            {Graphics::ShaderVariableType::Sampler, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER},
        };

    struct Range {
        decltype(parameterTable.begin()) iter;
        size_t                           count;
    };
    std::vector<Range> ranges;
    for (auto iter = parameterTable.begin(); iter != parameterTable.end(); iter++) {
        if (ranges.empty()) {
            ranges.emplace_back(Range{iter, 1});
            continue;
        }
        if (auto& p = *(ranges.back().iter);
            p.visibility == iter->visibility && p.type == iter->type && p.space == iter->space) {
            // new range
            if (p.registerIndex + ranges.back().count < iter->registerIndex)
                ranges.emplace_back(Range{iter, 1});
            // fllowing the range
            else if (p.registerIndex + ranges.back().count == iter->registerIndex)
                ranges.back().count++;
            // [Error] in the range
            else
                throw std::logic_error("Has a same paramter");
        } else {
            ranges.emplace_back(Range{iter, 1});
        }
    }
    // Now the range is sort by visibility, type, space, register
    // becase the data structure of parameters is set

    struct Table {
        Graphics::ShaderVisibility visibility;
        std::vector<size_t>        ranges;
    };
    std::vector<Table> tables;
    for (size_t i = 0; i < ranges.size(); i++) {
        if (tables.empty()) {
            tables.push_back(Table{ranges[i].iter->visibility, {i}});
            continue;
        }
        if (tables.back().visibility == ranges[i].iter->visibility) {
            if (ranges[i].iter->type == Graphics::ShaderVariableType::Sampler &&
                ranges[i].iter->type != ranges[tables.back().ranges.back()].iter->type) {
                // Sampler table
                tables.push_back(Table{ranges[i].iter->visibility, {i}});
            } else {
                tables.back().ranges.push_back(i);
            }
        }
    }

    m_RootSignatures.emplace(signature.Id(), RootSignature(tables.size()));
    auto& sig = m_RootSignatures[signature.Id()];
    for (size_t i = 0; i < tables.size(); i++) {
        sig[i].InitAsDescriptorTable(tables[i].ranges.size(), visibilityCast.at(tables[i].visibility));
        size_t offset = 0;
        for (size_t j = 0; j < tables[i].ranges.size(); j++) {
            auto& range = ranges[tables[i].ranges[j]];
            sig[i].SetTableRange(
                j,
                typeCast.at(range.iter->type),
                range.iter->registerIndex,
                range.count,
                range.iter->space);
            auto iter = range.iter;
            for (size_t k = 0; k < range.count; k++, iter++)
                sig.UpdateParameterInfo(iter->name, i, offset++);
        }
    }
    sig.Finalize(m_Device.Get(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void DX12DriverAPI::DeleteRootSignature(const Graphics::RootSignature& signature) {
    m_RootSignatures.erase(signature.Id());
}

void DX12DriverAPI::CreatePipelineState(const Graphics::PipelineState& pso) {
    m_Pso.emplace(pso.GetName(), GraphicsPSO());
    auto&                                 gpso = m_Pso[pso.GetName()];
    auto                                  vs   = pso.GetVS();
    auto                                  ps   = pso.GetPS();
    auto&                                 sig  = m_RootSignatures.at(pso.GetRootSignature()->Id());
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputDesc;
    for (auto&& layout : pso.GetInputLayout()) {
        D3D12_INPUT_ELEMENT_DESC desc = {};
        desc.SemanticName             = layout.semanticName.data();
        desc.SemanticIndex            = layout.semanticIndex;
        desc.Format                   = ToDxgiFormat(layout.format);
        desc.InputSlot                = layout.inputSlot;
        desc.AlignedByteOffset        = layout.alignedByOffset;
        desc.InputSlotClass           = layout.instanceCount.has_value()
                                            ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                            : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc.InstanceDataStepRate     = layout.instanceCount.has_value() ? layout.instanceCount.value() : 0;
        inputDesc.emplace_back(std::move(desc));
    }

    gpso.SetVertexShader(CD3DX12_SHADER_BYTECODE{vs->GetData(), vs->GetDataSize()});
    gpso.SetPixelShader(CD3DX12_SHADER_BYTECODE{ps->GetData(), ps->GetDataSize()});
    gpso.SetInputLayout(inputDesc);
    gpso.SetRootSignature(sig);
    gpso.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    auto depth        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = pso.GetDepthBufferFormat() != Graphics::Format::UNKNOWN;
    gpso.SetDepthStencilState(depth);
    gpso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    auto raDesc                  = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    raDesc.FrontCounterClockwise = true;
    gpso.SetRasterizerState(raDesc);
    gpso.SetRenderTargetFormats({ToDxgiFormat(pso.GetRenderTargetFormat())}, ToDxgiFormat(pso.GetDepthBufferFormat()));
    gpso.SetSampleMask(UINT_MAX);
    gpso.Finalize(m_Device.Get());
}

void DX12DriverAPI::DeletePipelineState(const Graphics::PipelineState& pso) {
    m_Pso.erase(pso.GetName());
}

std::shared_ptr<IGraphicsCommandContext> DX12DriverAPI::GetGraphicsCommandContext() {
    return std::make_unique<GraphicsCommandContext>(*this);
}

void DX12DriverAPI::WaitFence(uint64_t fenceValue) {
    m_CommandManager.WaitForFence(fenceValue);
}

void DX12DriverAPI::IdleGPU() { m_CommandManager.IdleGPU(); }

// ----------------------------
//  For test
// ----------------------------
void DX12DriverAPI::test(Graphics::RenderTarget& rt, const Graphics::PipelineState& pso) {
}

}  // namespace Hitagi::Graphics::backend::DX12