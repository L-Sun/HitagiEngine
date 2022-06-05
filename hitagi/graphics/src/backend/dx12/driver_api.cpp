#include "driver_api.hpp"

#include "gpu_buffer.hpp"
#include "command_context.hpp"
#include "hitagi/core/memory_manager.hpp"
#include "hitagi/graphics/enums.hpp"
#include "hitagi/graphics/resource.hpp"
#include "hitagi/resource/enums.hpp"
#include "magic_enum.hpp"
#include "sampler.hpp"
#include "utils.hpp"

#include <dxgiformat.h>
#include <windef.h>
#include <algorithm>

namespace hitagi::graphics::backend::DX12 {

ComPtr<ID3D12DebugDevice1> DX12DriverAPI::sm_DebugInterface = nullptr;

DX12DriverAPI::DX12DriverAPI()
    : DriverAPI(APIType::DirectX12),
      m_RetireResources(retire_resource_cmp) {
    unsigned dxgi_factory_flags = 0;

#if defined(_Debug)

    // Enable d3d12 debug layer.
    {
        ComPtr<ID3D12Debug3> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            debug_controller->EnableDebugLayer();
            // Enable additional debug layers.
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif  // _Debug
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
}

size_t DX12DriverAPI::ResizeSwapChain(uint32_t width, uint32_t height) {
    assert(m_SwapChain && "No swap chain created.");
    DXGI_SWAP_CHAIN_DESC1 desc;
    m_SwapChain->GetDesc1(&desc);
    ThrowIfFailed(m_SwapChain->ResizeBuffers(desc.BufferCount, width, height, desc.Format, desc.Flags));

    return m_SwapChain->GetCurrentBackBufferIndex();
}

std::shared_ptr<graphics::RenderTarget> DX12DriverAPI::CreateRenderTarget(std::string_view name, const graphics::RenderTargetDesc& desc) {
    D3D12_RESOURCE_DESC d3d_desc = {};
    d3d_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d3d_desc.SampleDesc.Count    = 1;
    d3d_desc.SampleDesc.Quality  = 0;
    d3d_desc.Width               = desc.width;
    d3d_desc.Height              = desc.height;
    d3d_desc.DepthOrArraySize    = 1;
    d3d_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    d3d_desc.Format              = to_dxgi_format(desc.format);

    return std::make_shared<graphics::RenderTarget>(
        name,
        std::make_unique<RenderTarget>(
            name,
            m_Device.Get(),
            m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(),
            d3d_desc),
        desc);
}

std::shared_ptr<graphics::RenderTarget> DX12DriverAPI::CreateRenderFromSwapChain(size_t frame_index) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> res;
    m_SwapChain->GetBuffer(frame_index, IID_PPV_ARGS(&res));
    auto rt = std::make_unique<RenderTarget>(
        fmt::format("RT for SwapChain {}", frame_index),
        m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(),
        res.Detach());
    auto desc = rt->GetResource()->GetDesc();

    return std::make_shared<graphics::RenderTarget>(
        fmt::format("RT for SwapChain {}", frame_index),
        std::move(rt),
        graphics::RenderTargetDesc{
            .format = to_format(desc.Format),
            .width  = desc.Width,
            .height = desc.Height,
        });
}

std::shared_ptr<graphics::VertexBuffer> DX12DriverAPI::CreateVertexBuffer(std::shared_ptr<resource::VertexArray> vertices) {
    auto                       name = vertices->GetUniqueName();
    graphics::VertexBufferDesc desc = {.vertex_count = vertices->VertexCount(), .slot_mask = vertices->GetSlotMask()};

    auto result = std::make_shared<graphics::VertexBuffer>(name, nullptr, desc);
    auto vb     = std::make_unique<VertexBuffer>(m_Device.Get(), *result);
    result->SetResource(std::move(vb));

    GraphicsCommandContext context(*this);

    for (std::size_t slot = 0; slot < magic_enum::enum_count<resource::VertexAttribute>(); slot++) {
        if (desc.slot_mask.test(slot)) {
            const auto&       buffer = vertices->GetBuffer(slot);
            const std::size_t offset = result->desc.slot_offset[slot];
            context.UpdateBuffer(result, offset, buffer.GetData(), buffer.GetDataSize());
        }
    }

    context.Finish(true);
    return result;
}

std::shared_ptr<graphics::IndexBuffer> DX12DriverAPI::CreateIndexBuffer(std::shared_ptr<resource::IndexArray> indices) {
    auto name = indices->GetUniqueName();

    graphics::IndexBufferDesc desc = {
        .index_count = indices->IndexCount(),
        .index_size  = indices->IndexSize(),
    };

    auto result = std::make_shared<graphics::IndexBuffer>(name, nullptr, desc);
    auto buffer = std::make_unique<IndexBuffer>(m_Device.Get(), *result);
    result->SetResource(std::move(buffer));

    GraphicsCommandContext context(*this);
    context.UpdateBuffer(result, 0, indices->Buffer().GetData(), indices->Buffer().GetDataSize());
    context.Finish(true);

    return result;
}

std::shared_ptr<graphics::ConstantBuffer> DX12DriverAPI::CreateConstantBuffer(std::string_view name, graphics::ConstantBufferDesc desc) {
    auto result = std::make_shared<graphics::ConstantBuffer>(name, nullptr, desc);
    auto cb     = std::make_unique<ConstantBuffer>(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], *result);

    result->SetResource(std::move(cb));
    return result;
}

void DX12DriverAPI::ResizeConstantBuffer(std::shared_ptr<graphics::ConstantBuffer> buffer, size_t new_num_elements) {
    auto cb = buffer->GetBackend<ConstantBuffer>();
    cb->Resize(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], new_num_elements);
    const_cast<ConstantBufferDesc*>(&buffer->desc)->num_elements = new_num_elements;
}

std::shared_ptr<graphics::TextureBuffer> DX12DriverAPI::CreateTextureBuffer(std::shared_ptr<resource::Texture> texture) {
    auto name = texture->GetUniqueName();

    auto image = texture->GetTextureImage();

    graphics::TextureBufferDesc desc{
        .format = graphics::Format::R8G8B8A8_UNORM,
        .width  = image->Width(),
        .height = image->Height(),
        .pitch  = image->Pitch(),
    };
    auto result = CreateTextureBuffer(name, desc);

    CopyCommandContext     context(*this);
    D3D12_SUBRESOURCE_DATA sub_data;
    sub_data.pData      = image->Buffer().GetData();
    sub_data.RowPitch   = image->Pitch();
    sub_data.SlicePitch = image->Buffer().GetDataSize();
    context.InitializeTexture(*result->GetBackend<TextureBuffer>(), {sub_data});

    return result;
}

std::shared_ptr<graphics::TextureBuffer> DX12DriverAPI::CreateTextureBuffer(std::string_view name, graphics::TextureBufferDesc desc) {
    D3D12_RESOURCE_DESC d3d_desc = {};
    d3d_desc.Width               = desc.width;
    d3d_desc.Height              = desc.height;
    // TODO support diffrent format
    d3d_desc.Format             = to_dxgi_format(desc.format);
    d3d_desc.MipLevels          = desc.mip_level;
    d3d_desc.DepthOrArraySize   = 1;
    d3d_desc.SampleDesc.Count   = desc.sample_count;
    d3d_desc.SampleDesc.Quality = desc.sample_quality;
    d3d_desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto buffer = std::make_unique<TextureBuffer>(name, m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(), d3d_desc);
    return std::make_shared<graphics::TextureBuffer>(name, std::move(buffer), desc);
}

std::shared_ptr<graphics::DepthBuffer> DX12DriverAPI::CreateDepthBuffer(std::string_view name, const graphics::DepthBufferDesc& desc) {
    auto d3d_desc  = CD3DX12_RESOURCE_DESC::Tex2D(to_dxgi_format(desc.format), desc.width, desc.height);
    d3d_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    auto db = std::make_unique<DepthBuffer>(
        name, m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate(),
        d3d_desc, desc.clear_depth, desc.clear_stencil);

    return std::make_shared<graphics::DepthBuffer>(name, std::move(db), desc);
}

void DX12DriverAPI::UpdateConstantBuffer(std::shared_ptr<graphics::ConstantBuffer> buffer, std::size_t index, const std::byte* data, size_t size) {
    buffer->GetBackend<ConstantBuffer>()->UpdateData(index, data, size);
}

void DX12DriverAPI::RetireResource(std::shared_ptr<graphics::Resource> resource, std::uint64_t fence_value) {
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.top().first))
        m_RetireResources.pop();

    m_RetireResources.emplace(fence_value, std::move(resource));
}

// TODO: Custom sampler
std::shared_ptr<graphics::Sampler> DX12DriverAPI::CreateSampler(std::string_view name, const resource::SamplerDesc& desc) {
    return std::make_shared<graphics::Sampler>(
        name,
        std::make_unique<Sampler>(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate(), to_d3d_sampler_desc(desc)),
        desc);
}

void DX12DriverAPI::CreateRootSignature(std::shared_ptr<graphics::RootSignature> signature) {
    static const std::unordered_map<graphics::ShaderVariableType, D3D12_DESCRIPTOR_RANGE_TYPE>
        type_cast = {
            {graphics::ShaderVariableType::CBV, D3D12_DESCRIPTOR_RANGE_TYPE_CBV},
            {graphics::ShaderVariableType::SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV},
            {graphics::ShaderVariableType::UAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV},
            {graphics::ShaderVariableType::Sampler, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER},
        };

    // copy
    auto parameters = signature->GetParametes();
    if (parameters.empty()) {
        throw std::invalid_argument("can not create a empty root signature!");
    }
    std::sort(parameters.begin(), parameters.end(), [](const auto& lhs, const auto& rhs) {
        if (auto cmp = lhs.visibility <=> rhs.visibility; cmp != 0)
            return cmp < 0;
        if (auto cmp = lhs.type <=> rhs.type; cmp != 0)
            return cmp < 0;
        if (auto cmp = lhs.space <=> rhs.space; cmp != 0)
            return cmp < 0;
        return (lhs.register_index <=> rhs.register_index) < 0;
    });

    // [begin, end)
    std::vector<std::pair<size_t, size_t>> tables = {std::make_pair(0, 0)};
    std::vector<std::pair<size_t, size_t>> ranges = {std::make_pair(0, 0)};
    for (size_t i = 0; i < parameters.size(); i++) {
        if (parameters[tables.back().first].visibility == parameters[i].visibility) {
            if (parameters[i].type != graphics::ShaderVariableType::Sampler)
                tables.back().second++;
            else if (parameters[tables.back().first].type == parameters[i].type)
                tables.back().second++;
            else
                tables.emplace_back(i, i + 1);
        } else {
            tables.emplace_back(i, i + 1);
        }

        if (parameters[ranges.back().first].type == parameters[i].type &&
            parameters[ranges.back().first].space == parameters[i].space) {
            ranges.back().second++;
        } else {
            ranges.emplace_back(i, i + 1);
        }
    }

    const auto& static_sampler_descs = signature->GetStaticSamplerDescs();

    auto sig = std::make_unique<RootSignature>(signature->GetName(), static_cast<uint32_t>(tables.size()), static_sampler_descs.size());
    // Init Static Samplers
    for (const auto& static_sampler_desc : static_sampler_descs) {
        sig->InitStaticSampler(static_sampler_desc.register_index,
                               static_sampler_desc.space,
                               to_d3d_sampler_desc(static_sampler_desc.desc),
                               to_d3d_shader_visibility(static_sampler_desc.visibility));
    }

    // Init parameters
    for (size_t table_index = 0, range_index = 0; table_index < tables.size(); table_index++) {
        size_t num_ranges = 0;
        while (
            range_index < ranges.size() &&
            // check the range is or not in the table
            tables[table_index].first <= ranges[range_index].first && ranges[range_index].second <= tables[table_index].second) {
            num_ranges++;
            range_index++;
        }

        (*sig)[table_index].InitAsDescriptorTable(num_ranges, to_d3d_shader_visibility(parameters[tables[table_index].first].visibility));

        range_index -= num_ranges;
        for (size_t range_index_in_table = 0; range_index_in_table < num_ranges; range_index_in_table++) {
            auto [begin, end] = ranges[range_index++];
            (*sig)[table_index].SetTableRange(
                range_index_in_table,
                type_cast.at(parameters[begin].type),
                parameters[begin].register_index,
                end - begin,
                parameters[begin].space);

            while (begin < end) {
                // parameter index in table
                sig->UpdateParameterInfo(parameters[begin].name, table_index, begin - tables[table_index].first);
                begin++;
            }
        }
    }

    sig->Finalize(m_Device.Get(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    signature->SetResource(std::move(sig));
}

void DX12DriverAPI::CreatePipelineState(std::shared_ptr<graphics::PipelineState> pso) {
    auto gpso = std::make_unique<GraphicsPSO>(pso->GetName());
    auto vs   = pso->vertex_shader;
    auto ps   = pso->pixel_shader;
    auto sig  = pso->root_signature->GetBackend<RootSignature>();

    std::vector<D3D12_INPUT_ELEMENT_DESC> input_desc;
    for (std::size_t slot = 0; slot < pso->vertex_slot_mask.size(); slot++) {
        if (!pso->vertex_slot_mask.test(slot)) continue;

        auto                     attr = magic_enum::enum_cast<resource::VertexAttribute>(slot).value();
        D3D12_INPUT_ELEMENT_DESC desc = {};
        desc.SemanticName             = hlsl_semantic_name(attr);
        desc.SemanticIndex            = hlsl_semantic_index(attr);
        desc.Format                   = hlsl_semantic_format(attr);
        desc.InputSlot                = slot;
        desc.AlignedByteOffset        = D3D12_APPEND_ALIGNED_ELEMENT;
        desc.InputSlotClass           = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc.InstanceDataStepRate     = 0;
        input_desc.emplace_back(desc);
    }

    gpso->SetVertexShader(CD3DX12_SHADER_BYTECODE{vs.GetData(), vs.GetDataSize()});
    gpso->SetPixelShader(CD3DX12_SHADER_BYTECODE{ps.GetData(), ps.GetDataSize()});
    gpso->SetInputLayout(input_desc);
    gpso->SetRootSignature(*sig);
    gpso->SetBlendState(to_d3d_blend_desc(pso->blend_state));

    auto depth        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = pso->depth_buffer_format != graphics::Format::UNKNOWN;
    gpso->SetDepthStencilState(depth);
    gpso->SetPrimitiveTopologyType(to_dx_topology_type(pso->primitive_type));
    gpso->SetRasterizerState(to_d3d_rasterizer_desc(pso->rasterizer_state));
    gpso->SetRenderTargetFormats({to_dxgi_format(pso->render_format)}, to_dxgi_format(pso->depth_buffer_format));
    gpso->SetSampleMask(UINT_MAX);

    gpso->Finalize(m_Device.Get());

    pso->SetResource(std::move(gpso));
}

std::shared_ptr<IGraphicsCommandContext> DX12DriverAPI::GetGraphicsCommandContext() {
    return std::make_unique<GraphicsCommandContext>(*this);
}

bool DX12DriverAPI::IsFenceComplete(std::uint64_t fence_value) {
    return m_CommandManager.IsFenceComplete(fence_value);
}

void DX12DriverAPI::WaitFence(std::uint64_t fence_value) {
    m_CommandManager.WaitForFence(fence_value);
}

void DX12DriverAPI::IdleGPU() {
    m_CommandManager.IdleGPU();
}

}  // namespace hitagi::graphics::backend::DX12