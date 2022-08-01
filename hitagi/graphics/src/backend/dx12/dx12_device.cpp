#include "dx12_device.hpp"
#include "gpu_buffer.hpp"
#include "command_context.hpp"
#include "sampler.hpp"
#include "utils.hpp"

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/gpu_resource.hpp>

#include <d3d12.h>
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
    m_Resources.clear();

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

void DX12Device::CreateSwapChain(uint32_t width, uint32_t height, unsigned frame_count, resource::Format format, void* window) {
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

void DX12Device::InitRenderFromSwapChain(graphics::RenderTarget& rt, std::size_t frame_index) {
    assert(m_SwapChain && "No swap chain created.");
    ComPtr<ID3D12Resource> res;
    m_SwapChain->GetBuffer(frame_index, IID_PPV_ARGS(&res));
    auto desc = res->GetDesc();

    rt.name   = fmt::format("CreateFromSwapChain-{}", frame_index);
    rt.format = to_format(desc.Format);
    rt.width  = desc.Width,
    rt.height = desc.Height;

    auto&& [iter, success] = m_Resources.emplace(std::make_shared<RenderTarget>(
        this,
        fmt::format("RT for SwapChain {}", frame_index),
        res.Detach()));
    rt.gpu_resource        = *iter;
}

void DX12Device::InitRenderTarget(graphics::RenderTarget& rt) {
    D3D12_RESOURCE_DESC d3d_desc = {};
    d3d_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d3d_desc.SampleDesc.Count    = 1;
    d3d_desc.SampleDesc.Quality  = 0;
    d3d_desc.Width               = rt.width;
    d3d_desc.Height              = rt.height;
    d3d_desc.DepthOrArraySize    = 1;
    d3d_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    d3d_desc.Format              = to_dxgi_format(rt.format);

    auto&& [iter, success] = m_Resources.emplace(std::make_shared<RenderTarget>(this, rt.name, d3d_desc));
    rt.gpu_resource        = *iter;
}

void DX12Device::InitVertexBuffer(resource::VertexArray& vertices) {
    if (vertices.cpu_buffer.GetDataSize() == 0) return;

    auto&& [iter, success] = m_Resources.emplace(std::make_shared<GpuBuffer>(
        this,
        vertices.name,
        vertices.cpu_buffer.GetDataSize(),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
    vertices.gpu_resource  = *iter;
}

void DX12Device::InitIndexBuffer(resource::IndexArray& indices) {
    if (indices.cpu_buffer.GetDataSize() == 0) return;

    auto&& [iter, success] = m_Resources.emplace(std::make_shared<GpuBuffer>(
        this,
        indices.name,
        indices.cpu_buffer.GetDataSize(),
        D3D12_RESOURCE_STATE_INDEX_BUFFER));
    indices.gpu_resource   = *iter;
}

void DX12Device::InitTexture(resource::Texture& texture) {
    D3D12_RESOURCE_DESC d3d_desc = {};
    d3d_desc.Width               = texture.width;
    d3d_desc.Height              = texture.height;
    // TODO support diffrent format
    d3d_desc.Format             = to_dxgi_format(texture.format);
    d3d_desc.MipLevels          = texture.mip_level;
    d3d_desc.DepthOrArraySize   = 1;
    d3d_desc.SampleDesc.Count   = texture.sample_count;
    d3d_desc.SampleDesc.Quality = texture.sample_quality;
    d3d_desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto&& [iter, success] = m_Resources.emplace(std::make_shared<Texture>(
        this,
        texture.name,
        d3d_desc));
    texture.gpu_resource   = *iter;

    CopyCommandContext     context(this);
    D3D12_SUBRESOURCE_DATA sub_data;
    sub_data.pData      = texture.cpu_buffer.GetData();
    sub_data.RowPitch   = texture.pitch;
    sub_data.SlicePitch = texture.cpu_buffer.GetDataSize();
    context.InitializeTexture(texture, {sub_data});
    texture.dirty = false;
}

void DX12Device::InitConstantBuffer(graphics::ConstantBuffer& cb) {
    auto&& [iter, success] = m_Resources.emplace(std::make_shared<ConstantBuffer>(this, cb.name, cb));
    cb.gpu_resource        = *iter;
}

void DX12Device::InitDepthBuffer(graphics::DepthBuffer& db) {
    auto d3d_desc  = CD3DX12_RESOURCE_DESC::Tex2D(to_dxgi_format(db.format), db.width, db.height);
    d3d_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    auto&& [iter, success] = m_Resources.emplace(std::make_shared<DepthBuffer>(this, db.name, d3d_desc, db.clear_depth, db.clear_stencil));
    db.gpu_resource        = *iter;
}

// TODO: Custom sampler
void DX12Device::InitSampler(graphics::Sampler& sampler) {
    auto&& [iter, success] = m_Resources.emplace(std::make_shared<Sampler>(this, to_d3d_sampler_desc(sampler)));
    sampler.gpu_resource   = *iter;
}

void DX12Device::InitPipelineState(graphics::PipelineState& pipeline) {
    auto gpso = std::make_shared<GraphicsPSO>(pipeline.name, m_Device.Get());

    for (const auto& shader : {pipeline.vs, pipeline.ps}) {
        if (shader != nullptr) {
            if (shader->Program().Empty()) CompileShader(shader);
            gpso->SetShader(shader);
        }
    }
    assert(gpso->GetRootSignature() != nullptr);

    auto depth        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = pipeline.depth_buffer_format != resource::Format::UNKNOWN;

    (*gpso)
        .SetInputLayout(CreateInputLayout(pipeline.vs))
        .SetBlendState(to_d3d_blend_desc(pipeline.blend_state))
        .SetDepthStencilState(depth)
        .SetPrimitiveTopologyType(to_dx_topology_type(pipeline.primitive_type))
        .SetRasterizerState(to_d3d_rasterizer_desc(pipeline.rasterizer_state))
        .SetRenderTargetFormats({to_dxgi_format(pipeline.render_format)}, to_dxgi_format(pipeline.depth_buffer_format))
        .SetSampleMask(UINT_MAX)
        .Finalize();

    auto&& [iter, success] = m_Resources.emplace(gpso);
    pipeline.gpu_resource  = *iter;
}

void DX12Device::ResizeConstantBuffer(graphics::ConstantBuffer& buffer, size_t new_num_elements) {
    if (buffer.gpu_resource.lock() == nullptr) {
        spdlog::get("GraphicsManager")->error("Can not resize an uninitialized constant buffer ({})!", buffer.name);
        return;
    }
    auto cb = buffer.gpu_resource.lock()->GetBackend<ConstantBuffer>();
    cb->Resize(new_num_elements);
    buffer.num_elements = new_num_elements;
}

void DX12Device::UpdateConstantBuffer(graphics::ConstantBuffer& buffer, std::size_t index, const std::byte* data, size_t size) {
    if (buffer.gpu_resource.lock() == nullptr) {
        spdlog::get("GraphicsManager")->error("Can not update an uninitialized constant buffer ({})!", buffer.name);
        return;
    }
    buffer.gpu_resource.lock()->GetBackend<ConstantBuffer>()->UpdateData(index, data, size);
}

void DX12Device::RetireResource(std::shared_ptr<backend::Resource> resource) {
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.top()->fence_value)) {
        m_Resources.erase(m_RetireResources.top());
        m_RetireResources.pop();
    }

    m_RetireResources.emplace(std::move(resource));
}

std::shared_ptr<IGraphicsCommandContext> DX12Device::CreateGraphicsCommandContext() {
    return std::make_unique<GraphicsCommandContext>(this);
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

bool DX12Device::IsFenceComplete(std::uint64_t fence_value) {
    return m_CommandManager.IsFenceComplete(fence_value);
}

void DX12Device::WaitFence(std::uint64_t fence_value) {
    m_CommandManager.WaitForFence(fence_value);
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.top()->fence_value)) {
        m_Resources.erase(m_RetireResources.top());
        m_RetireResources.pop();
    }
}

void DX12Device::IdleGPU() {
    m_CommandManager.IdleGPU();
    while (!m_RetireResources.empty() && m_CommandManager.IsFenceComplete(m_RetireResources.top()->fence_value)) {
        m_Resources.erase(m_RetireResources.top());
        m_RetireResources.pop();
    }
}

DescriptorAllocator& DX12Device::GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    return m_DescriptorAllocator[type];
}

}  // namespace hitagi::graphics::backend::DX12