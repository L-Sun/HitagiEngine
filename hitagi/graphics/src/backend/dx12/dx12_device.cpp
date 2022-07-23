#include "dx12_device.hpp"
#include "gpu_buffer.hpp"
#include "command_context.hpp"
#include "sampler.hpp"
#include "utils.hpp"

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/resource.hpp>
#include <hitagi/resource/enums.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::graphics::backend::DX12 {

ComPtr<ID3D12DebugDevice1> DX12Device::sm_DebugInterface = nullptr;

DX12Device::DX12Device()
    : DeviceAPI(APIType::DirectX12),
      m_Logger(spdlog::get("GraphicsManager")),
      m_RetireResources(retire_resource_cmp) {
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
#endif  // _DEBUG
    ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_DxgiFactory)));

    // Create device.
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&m_Device)))) {
            ComPtr<IDXGIAdapter4> p_warpa_adapter;
            ThrowIfFailed(m_DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&p_warpa_adapter)));
            ThrowIfFailed(D3D12CreateDevice(p_warpa_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_Device)));
        }
    }

    // Create Shader utils
    {
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Utils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_ShaderCompiler)));
    }

    // Initialize command manager
    m_CommandManager.Initialize(m_Device.Get());
    // Iniaizlie Descriptor Allocator
    for (auto&& allocator : m_DescriptorAllocator)
        allocator.Initialize(m_Device.Get());

    ResourceBinder::ResetHeapPool();
}

DX12Device::~DX12Device() {
    m_CommandManager.IdleGPU();
    // Release the static variable that is allocation of DX12
    LinearAllocator::Destroy();
    ResourceBinder::ResetHeapPool();
#ifdef _DEBUG
    ThrowIfFailed(m_Device->QueryInterface(sm_DebugInterface.ReleaseAndGetAddressOf()));
#endif
}

void DX12Device::ReportDebugLog() {
#ifdef _DEBUG
    sm_DebugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif
}

void DX12Device::Present(size_t frame_index) {
    ThrowIfFailed(m_SwapChain->Present(0, 0));
}

void DX12Device::CreateSwapChain(uint32_t width, uint32_t height, unsigned frame_count, Format format, void* window) {
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

size_t DX12Device::ResizeSwapChain(uint32_t width, uint32_t height) {
    assert(m_SwapChain && "No swap chain created.");
    DXGI_SWAP_CHAIN_DESC1 desc;
    m_SwapChain->GetDesc1(&desc);
    ThrowIfFailed(m_SwapChain->ResizeBuffers(desc.BufferCount, width, height, desc.Format, desc.Flags));

    return m_SwapChain->GetCurrentBackBufferIndex();
}

std::shared_ptr<graphics::RenderTarget> DX12Device::CreateRenderTarget(std::string_view name, const graphics::RenderTargetDesc& desc) {
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
            m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(Descriptor::Type::RTV),
            d3d_desc),
        desc);
}

std::shared_ptr<graphics::RenderTarget> DX12Device::CreateRenderFromSwapChain(size_t frame_index) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> res;
    m_SwapChain->GetBuffer(frame_index, IID_PPV_ARGS(&res));
    auto rt = std::make_unique<RenderTarget>(
        fmt::format("RT for SwapChain {}", frame_index),
        m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(Descriptor::Type::RTV),
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

std::shared_ptr<graphics::VertexBuffer> DX12Device::CreateVertexBuffer(std::shared_ptr<resource::VertexArray> vertices) {
    auto                       name = vertices->GetUniqueName();
    graphics::VertexBufferDesc desc = {.vertex_count = vertices->VertexCount(), .attr_mask = vertices->GetAttributeMask()};

    auto result = std::make_shared<graphics::VertexBuffer>(name, nullptr, desc);
    auto vb     = std::make_unique<VertexBuffer>(m_Device.Get(), *result);
    result->SetResource(std::move(vb));

    GraphicsCommandContext context(*this);

    magic_enum::enum_for_each<resource::VertexAttribute>([&](auto attr) {
        auto vb = result->GetBackend<VertexBuffer>();
        if (vb->AttributeEnabled(attr())) {
            const auto& buffer = vertices->GetBuffer(attr());
            context.UpdateBuffer(
                result,
                vb->GetAttributeOffset(attr()),
                buffer.GetData(),
                buffer.GetDataSize());
        }
    });

    context.Finish(true);
    return result;
}

std::shared_ptr<graphics::IndexBuffer> DX12Device::CreateIndexBuffer(std::shared_ptr<resource::IndexArray> indices) {
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

std::shared_ptr<graphics::ConstantBuffer> DX12Device::CreateConstantBuffer(std::string_view name, graphics::ConstantBufferDesc desc) {
    auto result = std::make_shared<graphics::ConstantBuffer>(name, nullptr, desc);
    auto cb     = std::make_unique<ConstantBuffer>(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], *result);

    result->SetResource(std::move(cb));
    return result;
}

void DX12Device::ResizeConstantBuffer(std::shared_ptr<graphics::ConstantBuffer> buffer, size_t new_num_elements) {
    auto cb = buffer->GetBackend<ConstantBuffer>();
    cb->Resize(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], new_num_elements);
    const_cast<ConstantBufferDesc*>(&buffer->desc)->num_elements = new_num_elements;
}

std::shared_ptr<graphics::TextureBuffer> DX12Device::CreateTextureBuffer(std::shared_ptr<resource::Texture> texture) {
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

std::shared_ptr<graphics::TextureBuffer> DX12Device::CreateTextureBuffer(std::string_view name, graphics::TextureBufferDesc desc) {
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

    auto buffer = std::make_unique<TextureBuffer>(name, m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(Descriptor::Type::SRV), d3d_desc);
    return std::make_shared<graphics::TextureBuffer>(name, std::move(buffer), desc);
}

std::shared_ptr<graphics::DepthBuffer> DX12Device::CreateDepthBuffer(std::string_view name, const graphics::DepthBufferDesc& desc) {
    auto d3d_desc  = CD3DX12_RESOURCE_DESC::Tex2D(to_dxgi_format(desc.format), desc.width, desc.height);
    d3d_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    auto db = std::make_unique<DepthBuffer>(
        name, m_Device.Get(),
        m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate(Descriptor::Type::DSV),
        d3d_desc, desc.clear_depth, desc.clear_stencil);

    return std::make_shared<graphics::DepthBuffer>(name, std::move(db), desc);
}

void DX12Device::UpdateConstantBuffer(std::shared_ptr<graphics::ConstantBuffer> buffer, std::size_t index, const std::byte* data, size_t size) {
    buffer->GetBackend<ConstantBuffer>()->UpdateData(index, data, size);
}

void DX12Device::RetireResource(std::shared_ptr<graphics::Resource> resource, std::uint64_t fence_value) {
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.top().first))
        m_RetireResources.pop();

    m_RetireResources.emplace(fence_value, std::move(resource));
}

// TODO: Custom sampler
std::shared_ptr<graphics::Sampler> DX12Device::CreateSampler(std::string_view name, const resource::SamplerDesc& desc) {
    return std::make_shared<graphics::Sampler>(
        name,
        std::make_unique<Sampler>(m_Device.Get(), m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate(Descriptor::Type::Sampler), to_d3d_sampler_desc(desc)),
        desc);
}

void DX12Device::CompileShader(const std::shared_ptr<resource::Shader>& shader) {
    std::pmr::vector<const wchar_t*> args;
    switch (shader->GetType()) {
        case resource::Shader::Type::Vertex:
            args = {shader->GetPath().c_str(), L"-E", L"VSMain", L"-T", L"vs_6_4"};
            break;
        case resource::Shader::Type::Pixel:
            args = {shader->GetPath().c_str(), L"-E", L"PSMain", L"-T", L"ps_6_4"};
            break;
        case resource::Shader::Type::Geometry:
            args = {shader->GetPath().c_str(), L"-E", L"GSMain", L"-T", L"gs_6_4"};
            break;
        case resource::Shader::Type::Compute:
            args = {shader->GetPath().c_str(), L"-E", L"CSMain", L"-T", L"cs_6_4"};
            break;
        default: {
            m_Logger->warn("Unsupported shader: {}", magic_enum::enum_name(shader->GetType()));
        }
    }
    args.emplace_back(L"-Zi");
    args.emplace_back(L"-Zpr");
#ifdef _DEBUG
    args.emplace_back(L"-O0");
    args.emplace_back(L"-Qembed_debug");
#endif
    std::pmr::string compile_command;
    for (std::pmr::wstring arg : args) {
        compile_command.append(arg.begin(), arg.end());
        compile_command.push_back(' ');
    }

    m_Logger->debug("Compile Shader: {}", compile_command);
    assert(!shader->SourceCode().Empty());

    ComPtr<IDxcIncludeHandler> include_handler;
    m_Utils->CreateDefaultIncludeHandler(&include_handler);

    DxcBuffer source_buffer{
        .Ptr      = shader->SourceCode().GetData(),
        .Size     = shader->SourceCode().GetDataSize(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<IDxcResult> compile_result;
    m_ShaderCompiler->Compile(
        &source_buffer,
        args.data(),
        args.size(),
        include_handler.Get(),
        IID_PPV_ARGS(&compile_result));

    ComPtr<IDxcBlobUtf8> compile_errors;
    compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compile_errors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (compile_errors != nullptr && compile_errors->GetStringLength() != 0)
        m_Logger->warn("Warnings and Errors:\n{}\n", compile_errors->GetStringPointer());

    HRESULT hr;
    compile_result->GetStatus(&hr);
    if (FAILED(hr)) {
        m_Logger->error("compilation Failed\n");
        return;
    }

    ComPtr<IDxcBlob>      shader_buffer;
    ComPtr<IDxcBlobUtf16> shader_name;

    compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_buffer), &shader_name);
    if (shader_buffer != nullptr) {
        shader->Program() = core::Buffer(shader_buffer->GetBufferPointer(), shader_buffer->GetBufferSize());
    }
}

std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> DX12Device::CreateInputLayout(const std::shared_ptr<resource::Shader>& vs) {
    assert(vs->GetType() == resource::Shader::Type::Vertex);

    if (vs->Program().Empty()) {
        CompileShader(vs);
        assert(!vs->Program().Empty());
    }

    DxcBuffer vs_buffer{
        .Ptr      = vs->Program().GetData(),
        .Size     = vs->Program().GetDataSize(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<ID3D12ShaderReflection> vs_reflection;
    m_Utils->CreateReflection(&vs_buffer, IID_PPV_ARGS(&vs_reflection));

    D3D12_SHADER_DESC shader_desc;
    vs_reflection->GetDesc(&shader_desc);

    std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> input_descs;
    for (std::size_t slot = 0; slot < shader_desc.InputParameters; slot++) {
        D3D12_SIGNATURE_PARAMETER_DESC vertex_attribute_desc;
        vs_reflection->GetInputParameterDesc(slot, &vertex_attribute_desc);

        D3D12_INPUT_ELEMENT_DESC desc;
        desc.SemanticName         = hlsl_semantic_name(vertex_attribute_desc.SemanticName);
        desc.SemanticIndex        = vertex_attribute_desc.SemanticIndex;
        desc.InputSlot            = slot;
        desc.Format               = to_dxgi_format(vertex_attribute_desc.ComponentType, vertex_attribute_desc.Mask);
        desc.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT;
        desc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc.InstanceDataStepRate = 0;

        input_descs.emplace_back(desc);
    }

    return input_descs;
}

std::shared_ptr<graphics::PipelineState> DX12Device::CreatePipelineState(std::string_view name, const graphics::PipelineStateDesc& desc) {
    auto gpso = std::make_unique<GraphicsPSO>(name, m_Device.Get());

    for (const auto& shader : {desc.vs, desc.ps}) {
        if (shader != nullptr) {
            if (shader->Program().Empty()) CompileShader(shader);
            gpso->SetShader(shader);
        }
    }
    assert(gpso->GetRootSignature() != nullptr);

    auto depth        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = desc.depth_buffer_format != graphics::Format::UNKNOWN;

    (*gpso)
        .SetInputLayout(CreateInputLayout(desc.vs))
        .SetBlendState(to_d3d_blend_desc(desc.blend_state))
        .SetDepthStencilState(depth)
        .SetPrimitiveTopologyType(to_dx_topology_type(desc.primitive_type))
        .SetRasterizerState(to_d3d_rasterizer_desc(desc.rasterizer_state))
        .SetRenderTargetFormats({to_dxgi_format(desc.render_format)}, to_dxgi_format(desc.depth_buffer_format))
        .SetSampleMask(UINT_MAX)
        .Finalize();

    return std::make_shared<graphics::PipelineState>(name, std::move(gpso), desc);
}

std::shared_ptr<IGraphicsCommandContext> DX12Device::GetGraphicsCommandContext() {
    return std::make_unique<GraphicsCommandContext>(*this);
}

bool DX12Device::IsFenceComplete(std::uint64_t fence_value) {
    return m_CommandManager.IsFenceComplete(fence_value);
}

void DX12Device::WaitFence(std::uint64_t fence_value) {
    m_CommandManager.WaitForFence(fence_value);
}

void DX12Device::IdleGPU() {
    m_CommandManager.IdleGPU();
}

}  // namespace hitagi::graphics::backend::DX12