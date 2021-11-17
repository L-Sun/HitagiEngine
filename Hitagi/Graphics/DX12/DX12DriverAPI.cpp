#include "DX12DriverAPI.hpp"

#include "GpuBuffer.hpp"
#include "CommandContext.hpp"
#include "Sampler.hpp"
#include "EnumConverter.hpp"

#include <windef.h>

namespace Hitagi::Graphics::backend::DX12 {
ComPtr<ID3D12DebugDevice1> DX12DriverAPI::sm_DebugInterface = nullptr;

DX12DriverAPI::DX12DriverAPI() : DriverAPI(APIType::DirectX12) {
    unsigned dxgi_factory_flags = 0;

#if defined(_DEBUG)

    // Enable d3d12 debug layer.
    {
        ComPtr<ID3D12Debug3> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            debug_controller->EnableDebugLayer();
            // Enable additional debug layers.
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif  // DEBUG
    ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_DxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&m_Device)))) {
            ComPtr<IDXGIAdapter4> p_warpa_adapter;
            ThrowIfFailed(m_DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&p_warpa_adapter)));
            ThrowIfFailed(D3D12CreateDevice(p_warpa_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_Device)));
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

void DX12DriverAPI::Present(size_t frame_index) {
    ThrowIfFailed(m_SwapChain->Present(0, 0));
}

void DX12DriverAPI::CreateSwapChain(uint32_t width, uint32_t height, unsigned frame_count, Format format, void* window) {
    auto h_wnd = *reinterpret_cast<HWND*>(window);

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.BufferCount           = frame_count;
    swap_chain_desc.Width                 = width;
    swap_chain_desc.Height                = height;
    swap_chain_desc.Format                = to_dxgi_format(format);
    swap_chain_desc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.Scaling               = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
    swap_chain_desc.SampleDesc.Count      = 1;
    swap_chain_desc.SampleDesc.Quality    = 0;
    swap_chain_desc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    ThrowIfFailed(m_DxgiFactory->CreateSwapChainForHwnd(m_CommandManager.GetCommandQueue(), h_wnd, &swap_chain_desc,
                                                        nullptr, nullptr, &swap_chain));

    ThrowIfFailed(swap_chain.As(&m_SwapChain));
    ThrowIfFailed(m_DxgiFactory->MakeWindowAssociation(h_wnd, DXGI_MWA_NO_ALT_ENTER));

    m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
}

std::shared_ptr<Graphics::Resource> DX12DriverAPI::GetSwapChainBuffer(size_t frame_index) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> res;
    m_SwapChain->GetBuffer(frame_index, IID_PPV_ARGS(&res));
    return std::make_shared<Graphics::Resource>("Swap chain buffer", std::make_unique<GpuResource>(res.Detach()));
}

std::shared_ptr<Graphics::RenderTarget> DX12DriverAPI::CreateRenderTarget(std::string_view name, const Graphics::RenderTarget::Description& desc) {
    D3D12_RESOURCE_DESC d3d_desc = {};
    d3d_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d3d_desc.SampleDesc.Count    = 1;
    d3d_desc.SampleDesc.Quality  = 0;
    d3d_desc.Width               = desc.width;
    d3d_desc.Height              = desc.height;
    d3d_desc.DepthOrArraySize    = 1;
    d3d_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    d3d_desc.Format              = to_dxgi_format(desc.format);

    return std::make_shared<Graphics::RenderTarget>(
        name,
        std::make_unique<RenderTarget>(
            name,
            m_Device.Get(),
            m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(),
            d3d_desc),
        desc);
}

std::shared_ptr<Graphics::RenderTarget> DX12DriverAPI::CreateRenderFromSwapChain(size_t frame_index) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> res;
    m_SwapChain->GetBuffer(frame_index, IID_PPV_ARGS(&res));
    auto rt = std::make_unique<RenderTarget>(
        "RT for SwapChain",
        m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(),
        res.Detach());
    auto desc = rt->GetResource()->GetDesc();

    return std::make_shared<Graphics::RenderTarget>(
        "RT for SwapChain",
        std::move(rt),
        Graphics::RenderTarget::Description{
            .format = to_format(desc.Format),
            .width  = desc.Width,
            .height = desc.Height,
        });
}

std::shared_ptr<Graphics::VertexBuffer> DX12DriverAPI::CreateVertexBuffer(std::string_view name, size_t vertex_count, size_t vertex_size, const uint8_t* initial_data) {
    auto buffer = std::make_unique<VertexBuffer>(m_Device.Get(), name, vertex_count, vertex_size);
    if (initial_data) {
        CopyCommandContext context(*this);
        context.InitializeBuffer(*buffer, initial_data, buffer->GetBufferSize());
    }
    return std::make_shared<Graphics::VertexBuffer>(name, std::move(buffer));
}

std::shared_ptr<Graphics::IndexBuffer> DX12DriverAPI::CreateIndexBuffer(std::string_view name, size_t index_count, size_t index_size, const uint8_t* initial_data) {
    auto buffer = std::make_unique<IndexBuffer>(m_Device.Get(), name, index_count, index_size);
    if (initial_data) {
        CopyCommandContext context(*this);
        context.InitializeBuffer(*buffer, initial_data, buffer->GetBufferSize());
    }
    return std::make_shared<Graphics::IndexBuffer>(name, std::move(buffer));
}

std::shared_ptr<Graphics::ConstantBuffer> DX12DriverAPI::CreateConstantBuffer(std::string_view name, size_t num_elements, size_t element_size) {
    return std::make_shared<Graphics::ConstantBuffer>(
        name,
        std::make_unique<ConstantBuffer>(
            name,
            m_Device.Get(),
            m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
            num_elements,
            element_size),
        num_elements,
        element_size);
}

void DX12DriverAPI::ResizeConstantBuffer(std::shared_ptr<Graphics::ConstantBuffer> buffer, size_t new_num_elements) {
    auto cb = buffer->GetBackend<ConstantBuffer>();
    cb->Resize(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], new_num_elements);
}

std::shared_ptr<Graphics::TextureBuffer> DX12DriverAPI::CreateTextureBuffer(std::string_view name, const Graphics::TextureBuffer::Description& desc) {
    D3D12_RESOURCE_DESC d3d_desc = {};
    d3d_desc.Width               = desc.width;
    d3d_desc.Height              = desc.height;
    d3d_desc.Format              = to_dxgi_format(desc.format);
    d3d_desc.MipLevels           = desc.mip_level;
    d3d_desc.DepthOrArraySize    = 1;
    d3d_desc.SampleDesc.Count    = desc.sample_count;
    d3d_desc.SampleDesc.Quality  = desc.sample_quality;
    d3d_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto buffer = std::make_unique<TextureBuffer>(
        name,
        m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(),
        d3d_desc);
    if (desc.initial_data) {
        CopyCommandContext     context(*this);
        D3D12_SUBRESOURCE_DATA sub_data;
        sub_data.pData      = desc.initial_data;
        sub_data.RowPitch   = desc.pitch;
        sub_data.SlicePitch = desc.initial_data_size;
        context.InitializeTexture(*buffer, {sub_data});
    }
    return std::make_shared<Graphics::TextureBuffer>(name, std::move(buffer), desc);
}

std::shared_ptr<Graphics::DepthBuffer> DX12DriverAPI::CreateDepthBuffer(std::string_view name, const Graphics::DepthBuffer::Description& desc) {
    auto d3d_desc  = CD3DX12_RESOURCE_DESC::Tex2D(to_dxgi_format(desc.format), desc.width, desc.height);
    d3d_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    return std::make_shared<Graphics::DepthBuffer>(
        name,
        std::make_unique<DepthBuffer>(
            name,
            m_Device.Get(),
            m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate(),
            d3d_desc,
            desc.clear_depth,
            desc.clear_stencil),
        desc);
}

void DX12DriverAPI::UpdateConstantBuffer(std::shared_ptr<Graphics::ConstantBuffer> buffer, size_t index, const uint8_t* data, size_t size) {
    buffer->GetBackend<ConstantBuffer>()->UpdateData(index, data, size);
}

void DX12DriverAPI::RetireResources(std::vector<std::shared_ptr<Graphics::Resource>> resources, uint64_t fence_value) {
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.front().first))
        m_RetireResources.pop();

    m_RetireResources.emplace(fence_value, std::move(resources));
}

// TODO: Custom sampler
std::shared_ptr<Graphics::Sampler> DX12DriverAPI::CreateSampler(std::string_view name, const Graphics::Sampler::Description& desc) {
    D3D12_SAMPLER_DESC sampler_desc = {};
    sampler_desc.AddressU           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.AddressV           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.AddressW           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.ComparisonFunc     = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler_desc.Filter             = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    sampler_desc.MaxAnisotropy      = 1;
    sampler_desc.MaxLOD             = D3D12_FLOAT32_MAX;
    sampler_desc.MinLOD             = 0;
    sampler_desc.MipLODBias         = 0.0f;

    return std::make_shared<Graphics::Sampler>(
        name,
        std::make_unique<Sampler>(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate(), sampler_desc),
        desc);
}

std::unique_ptr<backend::Resource> DX12DriverAPI::CreateRootSignature(const Graphics::RootSignature& signature) {
    auto& parameter_table = signature.GetParametes();
    if (parameter_table.empty()) {
        throw std::invalid_argument("can not create a empty root signature!");
    }

    constexpr auto num_visibility = static_cast<int>(Graphics::ShaderVisibility::Num_Visibility);
    constexpr auto num_var_type   = static_cast<int>(Graphics::ShaderVariableType::Num_Type);
    static const std::unordered_map<Graphics::ShaderVisibility, D3D12_SHADER_VISIBILITY>
        visibility_cast = {
            {Graphics::ShaderVisibility::All, D3D12_SHADER_VISIBILITY_ALL},
            {Graphics::ShaderVisibility::Vertex, D3D12_SHADER_VISIBILITY_VERTEX},
            {Graphics::ShaderVisibility::Hull, D3D12_SHADER_VISIBILITY_HULL},
            {Graphics::ShaderVisibility::Domain, D3D12_SHADER_VISIBILITY_DOMAIN},
            {Graphics::ShaderVisibility::Geometry, D3D12_SHADER_VISIBILITY_GEOMETRY},
            {Graphics::ShaderVisibility::Pixel, D3D12_SHADER_VISIBILITY_PIXEL},
        };
    static const std::unordered_map<Graphics::ShaderVariableType, D3D12_DESCRIPTOR_RANGE_TYPE>
        type_cast = {
            {Graphics::ShaderVariableType::CBV, D3D12_DESCRIPTOR_RANGE_TYPE_CBV},
            {Graphics::ShaderVariableType::SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV},
            {Graphics::ShaderVariableType::UAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV},
            {Graphics::ShaderVariableType::Sampler, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER},
        };

    struct Range {
        decltype(parameter_table.begin()) iter;
        size_t                            count{};
    };
    std::vector<Range> ranges;
    for (auto iter = parameter_table.begin(); iter != parameter_table.end(); iter++) {
        if (ranges.empty()) {
            ranges.emplace_back(Range{iter, 1});
            continue;
        }
        if (auto& p = *(ranges.back().iter);
            p.visibility == iter->visibility && p.type == iter->type && p.space == iter->space) {
            // new range
            if (p.register_index + ranges.back().count < iter->register_index)
                ranges.emplace_back(Range{iter, 1});
            // fllowing the range
            else if (p.register_index + ranges.back().count == iter->register_index)
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
        std::vector<size_t>        ranges{};
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

    auto sig = std::make_unique<RootSignature>(signature.GetName(), static_cast<uint32_t>(tables.size()));
    for (size_t i = 0; i < tables.size(); i++) {
        (*sig)[i].InitAsDescriptorTable(tables[i].ranges.size(), visibility_cast.at(tables[i].visibility));
        size_t offset = 0;
        for (size_t j = 0; j < tables[i].ranges.size(); j++) {
            auto& range = ranges[tables[i].ranges[j]];
            (*sig)[i].SetTableRange(
                j,
                type_cast.at(range.iter->type),
                range.iter->register_index,
                range.count,
                range.iter->space);
            auto iter = range.iter;
            for (size_t k = 0; k < range.count; k++, iter++)
                sig->UpdateParameterInfo(iter->name, i, offset++);
        }
    }
    sig->Finalize(m_Device.Get(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    return std::move(sig);
}

std::unique_ptr<backend::Resource> DX12DriverAPI::CreatePipelineState(const Graphics::PipelineState& pso) {
    auto gpso = std::make_unique<GraphicsPSO>(pso.GetName());
    auto vs   = pso.GetVS();
    auto ps   = pso.GetPS();
    auto sig  = pso.GetRootSignature()->GetBackend<RootSignature>();

    std::vector<D3D12_INPUT_ELEMENT_DESC> input_desc;
    for (auto&& layout : pso.GetInputLayout()) {
        D3D12_INPUT_ELEMENT_DESC desc = {};
        desc.SemanticName             = layout.semantic_name.data();
        desc.SemanticIndex            = layout.semantic_index;
        desc.Format                   = to_dxgi_format(layout.format);
        desc.InputSlot                = layout.input_slot;
        desc.AlignedByteOffset        = layout.aligned_by_offset;
        desc.InputSlotClass           = layout.instance_count.has_value()
                                            ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                            : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc.InstanceDataStepRate     = layout.instance_count.has_value() ? layout.instance_count.value() : 0;
        input_desc.emplace_back(std::move(desc));
    }

    gpso->SetVertexShader(CD3DX12_SHADER_BYTECODE{vs->GetData(), vs->GetDataSize()});
    gpso->SetPixelShader(CD3DX12_SHADER_BYTECODE{ps->GetData(), ps->GetDataSize()});
    gpso->SetInputLayout(input_desc);
    gpso->SetRootSignature(*sig);
    gpso->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    auto depth        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = pso.GetDepthBufferFormat() != Graphics::Format::UNKNOWN;
    gpso->SetDepthStencilState(depth);
    gpso->SetPrimitiveTopologyType(to_dx_topology_type(pso.GetPrimitiveType()));
    auto ra_desc                  = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    ra_desc.FrontCounterClockwise = true;
    gpso->SetRasterizerState(ra_desc);
    gpso->SetRenderTargetFormats({to_dxgi_format(pso.GetRenderTargetFormat())}, to_dxgi_format(pso.GetDepthBufferFormat()));
    gpso->SetSampleMask(UINT_MAX);
    gpso->Finalize(m_Device.Get());

    return std::move(gpso);
}

std::shared_ptr<IGraphicsCommandContext> DX12DriverAPI::GetGraphicsCommandContext() {
    return std::make_unique<GraphicsCommandContext>(*this);
}

void DX12DriverAPI::WaitFence(uint64_t fence_value) {
    m_CommandManager.WaitForFence(fence_value);
}

void DX12DriverAPI::IdleGPU() { m_CommandManager.IdleGPU(); }

// ----------------------------
//  For test
// ----------------------------
void DX12DriverAPI::Test(Graphics::RenderTarget& rt, const Graphics::PipelineState& pso) {
}

}  // namespace Hitagi::Graphics::backend::DX12