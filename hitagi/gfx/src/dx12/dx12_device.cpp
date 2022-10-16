#include "dx12_device.hpp"
#include "dx12_command_queue.hpp"
#include "dx12_command_context.hpp"
#include "dx12_resource.hpp"
#include "utils.hpp"
#include "d3dx12.h"
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <magic_enum.hpp>
#include <dxcapi.h>
#include <dxgiformat.h>
#include <fmt/color.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <concepts>
#include <algorithm>

namespace hitagi::gfx {

ComPtr<ID3D12DebugDevice1> g_debug_interface;

DX12Device::DX12Device(std::string_view name) : Device(Type::DX12, name) {
    unsigned dxgi_factory_flags = 0;

#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug5> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            m_Logger->debug("Enabled D3D12 debug layer.");

            debug_controller->EnableDebugLayer();
            // Enable additional debug layers.
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif  // _DEBUG

    ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_Factory)));

    m_Logger->debug("Create D3D12 device...");
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&m_Device)))) {
            ComPtr<IDXGIAdapter4> p_warpa_adapter;
            ThrowIfFailed(m_Factory->EnumWarpAdapter(IID_PPV_ARGS(&p_warpa_adapter)));
            ThrowIfFailed(D3D12CreateDevice(p_warpa_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_Device)));
        }

        if (!name.empty()) {
            m_Device->SetName(std::wstring(name.begin(), name.end()).c_str());
        }
    }

    m_Logger->debug("Create feature support checker...");
    {
        ThrowIfFailed(m_FeatureSupport.Init(m_Device.Get()));
    }

    IntegrateD3D12Logger();

    m_Logger->debug("Create DXCompiler utils...");
    // Create Shader utils
    {
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Utils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_ShaderCompiler)));
    }

    magic_enum::enum_for_each<CommandType>([this](auto type) {
        m_CommandQueues[type] = std::make_shared<DX12CommandQueue>(this, type, fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type())));
    });

    // Initial Descriptor Allocator
    magic_enum::enum_for_each<D3D12_DESCRIPTOR_HEAP_TYPE>([this](auto type) {
        m_DescriptorAllocators[type] = std::make_unique<DescriptorAllocator>(this, type, false);
    });

    // Initial Gpu Descriptor Allocator, cbv uav srv share same descriptor heap
    m_GpuDescriptorAllocators[0] = std::make_unique<DescriptorAllocator>(this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    m_GpuDescriptorAllocators[1] = std::make_unique<DescriptorAllocator>(this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true);
}

DX12Device::~DX12Device() {
    UnregisterIntegratedD3D12Logger();
#ifdef _DEBUG
    ThrowIfFailed(m_Device->QueryInterface(g_debug_interface.ReleaseAndGetAddressOf()));
    report_debug_error_after_destory_fn = []() { DX12Device::ReportDebugLog(); };
#endif
}

void DX12Device::ReportDebugLog() {
#ifdef _DEBUG
    g_debug_interface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif
}

void DX12Device::IntegrateD3D12Logger() {
    ComPtr<ID3D12InfoQueue1> info_queue;
    if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        m_Logger->debug("Enabled D3D12 debug logger");
        info_queue->RegisterMessageCallback(
            [](D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void* context) {
                auto p_this = reinterpret_cast<DX12Device*>(context);
                switch (severity) {
                    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                        p_this->m_Logger->critical(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_ERROR:
                        p_this->m_Logger->error(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_WARNING:
                        p_this->m_Logger->warn(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_INFO:
                    case D3D12_MESSAGE_SEVERITY_MESSAGE:
                        p_this->m_Logger->info(description);
                        break;
                }
            },
            D3D12_MESSAGE_CALLBACK_FLAG_NONE,
            this,
            &m_DebugCookie);
    }
}

void DX12Device::UnregisterIntegratedD3D12Logger() {
    ComPtr<ID3D12InfoQueue1> info_queue;
    if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        m_Logger->debug("Unable D3D12 debug logger");
        info_queue->UnregisterMessageCallback(m_DebugCookie);
    }
}

void DX12Device::WaitIdle() {
    for (auto& queue : m_CommandQueues) {
        queue->WaitIdle();
    }
}

auto DX12Device::GetCommandQueue(CommandType type) const -> CommandQueue* {
    return m_CommandQueues[type].get();
}

auto DX12Device::CreateCommandQueue(CommandType type, std::string_view name) -> std::shared_ptr<CommandQueue> {
    return std::make_shared<DX12CommandQueue>(this, type, name);
}

auto DX12Device::CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return std::make_shared<DX12GraphicsCommandContext>(this, name);
}

auto DX12Device::CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return std::make_shared<DX12ComputeCommandContext>(this, name);
}

auto DX12Device::CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    return std::make_shared<DX12CopyCommandContext>(this, name);
}

auto DX12Device::CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain> {
    if (desc.window_ptr == nullptr) {
        m_Logger->error("Failed to create swap chain beacuse the window_ptr is {}",
                        fmt::styled("nullptr", fmt::fg(fmt::color::red)));
        return nullptr;
    }
    HWND h_wnd = *reinterpret_cast<HWND*>(desc.window_ptr);
    if (!IsWindow(h_wnd)) {
        m_Logger->error("The window ptr({}) is not a valid window.", desc.window_ptr);
        // Remove retired window
        if (m_SwapChains.contains(desc.window_ptr)) {
            m_SwapChains.erase(desc.window_ptr);
        }
        return nullptr;
    }

    std::shared_ptr<DX12SwapChain> result = nullptr;
    if (m_SwapChains.contains(desc.window_ptr)) {
        result = m_SwapChains.at(desc.window_ptr);
        if (result->desc.format == desc.format &&
            result->desc.frame_count == desc.frame_count &&
            result->desc.sample_count == desc.sample_count) {
            if (result->desc.name != desc.name) {
                const_cast<SwapChain::Desc&>(result->desc).name = desc.name;
            }
            return result;
        } else {
            // Recreate swapchain
            const_cast<SwapChain::Desc&>(result->desc) = desc;
            WaitIdle();
            result->swap_chain = nullptr;
        }
    } else {
        result = std::make_shared<DX12SwapChain>(desc);
    }

    UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (!result->desc.vsync) {
        if (ComPtr<IDXGIFactory5> factory5; SUCCEEDED(m_Factory.As(&factory5))) {
            BOOL allow_tearing = false;
            factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
            if (allow_tearing) {
                flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
                result->allow_tearing = allow_tearing;
            }
        }
    }

    DXGI_SWAP_CHAIN_DESC1 d3d_desc = {
        .Format     = to_dxgi_format(desc.format),
        .Stereo     = false,
        .SampleDesc = {
            .Count   = desc.sample_count,
            .Quality = 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = desc.frame_count,
        .Scaling     = DXGI_SCALING_STRETCH,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags       = flags,
    };

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

    result->associated_queue = m_CommandQueues[CommandType::Graphics].get();
    auto d3d_direct_queue    = m_CommandQueues[CommandType::Graphics]->GetD3DCommandQueue();
    if (FAILED(m_Factory->CreateSwapChainForHwnd(d3d_direct_queue, h_wnd, &d3d_desc, nullptr, nullptr, &swap_chain))) {
        m_Logger->error("Failed to create swap chain: {}", desc.name);
        return nullptr;
    }
    if (FAILED(m_Factory->MakeWindowAssociation(h_wnd, DXGI_MWA_NO_ALT_ENTER))) {
        m_Logger->warn("Failed to make window association!");
    }

    if (FAILED(swap_chain.As(&result->swap_chain))) {
        m_Logger->error("Failed to create swap chain: {}", desc.name);
        return nullptr;
    }

    m_SwapChains.emplace(desc.window_ptr, result);

    return result;
}

auto DX12Device::CreateBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GpuBuffer> {
    if (utils::has_flag(desc.usages, GpuBuffer::UsageFlags::MapRead) &&
        utils::has_flag(desc.usages, GpuBuffer::UsageFlags::MapWrite)) {
        m_Logger->error("The GpuBuffer::UsageFlags can not be {} and {} at same time! Name: {}, the actual flags is {}",
                        fmt::styled(magic_enum::enum_name(GpuBuffer::UsageFlags::MapRead), fmt::fg(fmt::color::green)),
                        fmt::styled(magic_enum::enum_name(GpuBuffer::UsageFlags::MapWrite), fmt::fg(fmt::color::green)),
                        fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                        fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
        return nullptr;
    }

    CD3DX12_HEAP_PROPERTIES heap_props;
    D3D12_RESOURCE_STATES   initial_state = D3D12_RESOURCE_STATE_COMMON;

    if (utils::has_flag(desc.usages, GpuBuffer::UsageFlags::MapRead)) {
        if (desc.usages != (GpuBuffer::UsageFlags::MapRead | GpuBuffer::UsageFlags::CopyDst)) {
            m_Logger->error("The GpuBuffer usage flag {} can only be combined with {}. Name: {}, the actual flags: {}",
                            fmt::styled(magic_enum::enum_name(GpuBuffer::UsageFlags::MapRead), fmt::fg(fmt::color::green)),
                            fmt::styled(magic_enum::enum_name(GpuBuffer::UsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                            fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                            fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
            return nullptr;
        }
        heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        initial_state |= D3D12_RESOURCE_STATE_COPY_DEST;

    } else if (utils::has_flag(desc.usages, GpuBuffer::UsageFlags::MapWrite)) {
        if (utils::has_flag(desc.usages, GpuBuffer::UsageFlags::CopyDst)) {
            m_Logger->error("The GpuBuffer usage flag {} can not be combined with {}. Name: {}, the actual flags: {}",
                            fmt::styled(magic_enum::enum_name(GpuBuffer::UsageFlags::MapWrite), fmt::fg(fmt::color::green)),
                            fmt::styled(magic_enum::enum_name(GpuBuffer::UsageFlags::CopySrc), fmt::fg(fmt::color::green)),
                            fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                            fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
            return nullptr;
        }
        heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        initial_state |= D3D12_RESOURCE_STATE_GENERIC_READ;

    } else {
        heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    }

    auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(
        utils::has_flag(desc.usages, GpuBuffer::UsageFlags::Constant) ? utils::align(desc.size, 256) : desc.size,
        D3D12_RESOURCE_FLAG_NONE);

    ComPtr<ID3D12Resource> resource;

    if (FAILED(m_Device->CreateCommittedResource(
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            initial_state,
            nullptr,
            IID_PPV_ARGS(&resource)))) {
        m_Logger->warn("Failed to create gpu buffer: {}", desc.name);
        return nullptr;
    }

    resource->SetName(std::pmr::wstring(desc.name.begin(), desc.name.end()).data());

    std::byte* cpu_ptr = nullptr;
    if (utils::has_flag(desc.usages, GpuBuffer::UsageFlags::MapRead) ||
        utils::has_flag(desc.usages, GpuBuffer::UsageFlags::MapWrite)) {
        if (FAILED(resource->Map(0, nullptr, reinterpret_cast<void**>(&cpu_ptr)))) {
            m_Logger->warn("Failed to map gpu buffer: {}", desc.name);
            return nullptr;
        }
    }

    auto result      = std::make_shared<DX12ResourceWrapper<GpuBuffer>>(desc, cpu_ptr);
    result->resource = std::move(resource);
    result->state    = initial_state;

    if (!initial_data.empty()) {
        if (initial_data.size() > desc.size) {
            m_Logger->warn("the initial data size({}) is larger than gpu buffer({}) size({}), so the exceed data will not be copied!",
                           fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                           fmt::styled(desc.size, fmt::fg(fmt::color::green)),
                           fmt::styled(desc.name, fmt::fg(fmt::color::green)));
        }
        if (cpu_ptr) {
            std::memcpy(cpu_ptr, initial_data.data(), std::min(initial_data.size(), desc.size));
        } else {
            auto upload_buffer = CreateBuffer(
                {
                    .name   = "UploadBuffer",
                    .size   = std::min(initial_data.size(), desc.size),
                    .usages = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
                },
                {initial_data.data(), std::min(initial_data.size(), desc.size)});

            auto copy_context = CreateCopyContext("CreateBuffer");
            copy_context->CopyBuffer(*upload_buffer, 0, *result, 0, upload_buffer->desc.size);
            copy_context->End();

            auto          copy_queue  = m_CommandQueues[CommandType::Copy];
            std::uint64_t fence_value = copy_queue->Submit({copy_context.get()});
            copy_queue->WaitForFence(fence_value);
        }
    }

    return result;
}

auto DX12Device::CreateBufferView(GpuBufferView::Desc desc) -> std::shared_ptr<GpuBufferView> {
    if (desc.size == 0) {
        desc.size = desc.buffer.desc.size - desc.offset;
    }

    if (desc.stride == 0) {
        m_Logger->error("The stride of GpuBufferView(name: {}) must be non-zero!",
                        fmt::styled(desc.buffer.desc.name, fmt::fg(fmt::color::green)));
        return nullptr;
    }
    if (desc.stride > desc.size) {
        m_Logger->warn("The stride(value: {}) of GpuBufferView(name: {}) is truncated to its size(value: {})",
                       fmt::styled(desc.stride, fmt::fg(fmt::color::red)),
                       fmt::styled(desc.buffer.desc.name, fmt::fg(fmt::color::green)),
                       fmt::styled(desc.size, fmt::fg(fmt::color::green)));
        desc.stride = desc.size;
    }

    auto result          = std::make_shared<DX12GpuBufferView>(desc);
    auto buffer_location = static_cast<DX12ResourceWrapper<GpuBuffer>&>(result->desc.buffer).resource->GetGPUVirtualAddress() + result->desc.offset;

    if (utils::has_flag(result->desc.usages, GpuBufferView::UsageFlags::Vertex)) {
        result->vbv = D3D12_VERTEX_BUFFER_VIEW{
            .BufferLocation = buffer_location,
            .SizeInBytes    = static_cast<UINT>(result->desc.size),
            .StrideInBytes  = static_cast<UINT>(result->desc.stride),
        };
    }
    if (utils::has_flag(result->desc.usages, GpuBufferView::UsageFlags::Index)) {
        if (result->desc.stride != sizeof(std::uint16_t) && result->desc.stride != sizeof(std::uint32_t)) {
            m_Logger->error("the stride of GpuBufferView({}) for {} must be 16 bits or 32 bits, but get {} bits",
                            fmt::styled(result->desc.buffer.desc.name, fmt::fg(fmt::color::green)),
                            fmt::styled(magic_enum::enum_name(GpuBufferView::UsageFlags::Index), fmt::fg(fmt::color::green)),
                            fmt::styled(8 * result->desc.stride, fmt::fg(fmt::color::red)));
            return nullptr;
        }
        result->ibv = D3D12_INDEX_BUFFER_VIEW{
            .BufferLocation = buffer_location,
            .SizeInBytes    = static_cast<UINT>(result->desc.size),
            .Format         = result->desc.stride == sizeof(std::uint16_t) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
        };
    }
    if (utils::has_flag(result->desc.usages, GpuBufferView::UsageFlags::Constant)) {
        std::size_t element_count = result->desc.size / desc.stride;
        result->cbvs              = m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(element_count);
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(result->cbvs.cpu_handle);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{
            .BufferLocation = buffer_location,
            .SizeInBytes    = static_cast<UINT>(utils::align(result->desc.stride, 256)),
        };
        for (std::size_t i = 0; i < result->cbvs.num; i++) {
            m_Device->CreateConstantBufferView(&cbv_desc, handle);
            cbv_desc.BufferLocation += result->desc.stride;
            handle.Offset(result->cbvs.increament_size);
        }
    }

    return result;
}

auto DX12Device::CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.array_size == 0) {
        m_Logger->error("Can not create zero size texture, Name: {}, the actual size is ({} x {} x {})[{}]",
                        fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                        fmt::styled(desc.width, fmt::fg(fmt::color::red)),
                        fmt::styled(desc.height, fmt::fg(fmt::color::red)),
                        fmt::styled(desc.depth, fmt::fg(fmt::color::red)),
                        fmt::styled(desc.array_size, fmt::fg(fmt::color::red)));
        return nullptr;
    }

    D3D12_RESOURCE_FLAGS resource_flag = D3D12_RESOURCE_FLAG_NONE;
    if (utils::has_flag(desc.usages, Texture::UsageFlags::UAV)) {
        resource_flag |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (utils::has_flag(desc.usages, Texture::UsageFlags::RTV)) {
        resource_flag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (utils::has_flag(desc.usages, Texture::UsageFlags::DSV)) {
        if (utils::has_flag(desc.usages, Texture::UsageFlags::RTV) ||
            utils::has_flag(desc.usages, Texture::UsageFlags::UAV)) {
            m_Logger->error("Texture::UsageFlags can not set {} with {} and {}. Name: {}, the actual usage is {}",
                            fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                            fmt::styled(magic_enum::enum_name(Texture::UsageFlags::DSV), fmt::fg(fmt::color::green)),
                            fmt::styled(magic_enum::enum_name(Texture::UsageFlags::RTV), fmt::fg(fmt::color::green)),
                            fmt::styled(magic_enum::enum_name(Texture::UsageFlags::UAV), fmt::fg(fmt::color::green)),
                            fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
            return nullptr;
        }
        resource_flag |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    CD3DX12_RESOURCE_DESC resource_desc;
    // 1D Texture
    if (desc.height == 1 && desc.depth == 1) {
        resource_desc = CD3DX12_RESOURCE_DESC::Tex1D(
            to_dxgi_format(desc.format),
            desc.width,
            desc.array_size,
            desc.mip_levels,
            resource_flag);
    }
    // 2D Texture
    else if (desc.depth == 1) {
        resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(
            to_dxgi_format(desc.format),
            desc.width,
            desc.height,
            desc.array_size,
            desc.mip_levels,
            desc.sample_count,
            0,
            resource_flag);
    }
    // 3D Texture
    else {
        resource_desc = CD3DX12_RESOURCE_DESC::Tex3D(
            to_dxgi_format(desc.format),
            desc.width,
            desc.height,
            desc.depth,
            desc.mip_levels,
            resource_flag);
    }

    CD3DX12_CLEAR_VALUE optimized_clear_value;
    optimized_clear_value.Color[0]             = desc.clear_value.color[0];
    optimized_clear_value.Color[1]             = desc.clear_value.color[1];
    optimized_clear_value.Color[2]             = desc.clear_value.color[2];
    optimized_clear_value.Color[3]             = desc.clear_value.color[3];
    optimized_clear_value.DepthStencil.Depth   = desc.clear_value.depth;
    optimized_clear_value.DepthStencil.Stencil = desc.clear_value.stencil;
    optimized_clear_value.Format               = to_dxgi_format(desc.format);

    if (optimized_clear_value.Format == DXGI_FORMAT_R16_TYPELESS) {
        optimized_clear_value.Format = DXGI_FORMAT_D16_UNORM;
    } else if (optimized_clear_value.Format == DXGI_FORMAT_R32_TYPELESS) {
        optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
    } else if (optimized_clear_value.Format == DXGI_FORMAT_R32G8X24_TYPELESS) {
        optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    }
    bool use_clear_value = utils::has_flag(desc.usages, Texture::UsageFlags::RTV) || utils::has_flag(desc.usages, Texture::UsageFlags::DSV);

    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ComPtr<ID3D12Resource> resource;
    if (FAILED(m_Device->CreateCommittedResource(
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            use_clear_value ? &optimized_clear_value : nullptr,
            IID_PPV_ARGS(&resource)))) {
        m_Logger->warn("Failed to create texture. Name: {}",
                       fmt::styled(desc.name, fmt::fg(fmt::color::red)));
        return nullptr;
    }

    resource->SetName(std::pmr::wstring(desc.name.begin(), desc.name.end()).data());

    if (!initial_data.empty()) {
        auto upload_buffer = std::static_pointer_cast<DX12ResourceWrapper<GpuBuffer>>(CreateBuffer(
            {
                .name   = "UploadTexture",
                .size   = GetRequiredIntermediateSize(resource.Get(), 0, resource_desc.Subresources(m_Device.Get())),
                .usages = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
            }));

        D3D12_SUBRESOURCE_DATA textureData = {
            .pData      = initial_data.data(),
            .RowPitch   = static_cast<LONG_PTR>(desc.width * (get_format_bit_size(desc.format) >> 3)),
            .SlicePitch = textureData.RowPitch * desc.height,
        };

        auto copy_context = std::static_pointer_cast<DX12CopyCommandContext>(CreateCopyContext("UploadTexture"));
        auto cmd_list     = copy_context->GetCmdList();
        UpdateSubresources(cmd_list, resource.Get(), upload_buffer->resource.Get(), 0, 0, resource_desc.Subresources(m_Device.Get()), &textureData);
        copy_context->End();

        auto          copy_queue  = m_CommandQueues[CommandType::Copy];
        std::uint64_t fence_value = copy_queue->Submit({copy_context.get()});
        copy_queue->WaitForFence(fence_value);
    }

    auto result      = std::make_shared<DX12ResourceWrapper<Texture>>(desc);
    result->resource = resource;
    result->state    = D3D12_RESOURCE_STATE_COMMON;

    return result;
}

auto DX12Device::CreateTextureView(TextureView::Desc desc) -> std::shared_ptr<TextureView> {
    if (desc.format == Format::UNKNOWN) {
        desc.format = desc.textuer.desc.format;
    }

    auto result         = std::make_shared<DX12DescriptorWrapper<TextureView>>(desc);
    auto d3d_res        = static_cast<DX12ResourceWrapper<Texture>&>(result->desc.textuer).resource.Get();
    bool create_succeed = false;

    if (utils::has_flag(result->desc.textuer.desc.usages, Texture::UsageFlags::SRV)) {
        auto srv_desc = to_d3d_srv_desc(result->desc);
        result->srv   = m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();

        m_Device->CreateShaderResourceView(d3d_res, &srv_desc, result->srv.cpu_handle);
        create_succeed = true;
    }

    if (utils::has_flag(result->desc.textuer.desc.usages, Texture::UsageFlags::UAV)) {
        m_Logger->warn("Unimplement for {}",
                       fmt::styled(magic_enum::enum_name(Texture::UsageFlags::RTV), fmt::fg(fmt::color::red)));
        // auto uav_desc                              = to_d3d_uav_desc(result->desc);
        // result->descriptors[Descriptor::Type::UAV] = m_DescriptorAllocators[Descriptor::Type::UAV]->Allocate();

        // auto d3d_res = std::static_pointer_cast<DX12ResourceWrapper<Texture>>(result->desc.textuer)->resource.Get();
        // m_Device->CreateUnorderedAccessView(d3d_res, &uav_desc, result->descriptors[Descriptor::Type::UAV].handle);
    }

    if (utils::has_flag(result->desc.textuer.desc.usages, Texture::UsageFlags::RTV)) {
        auto rtv_desc = to_d3d_rtv_desc(result->desc);
        result->rtv   = m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate();

        m_Device->CreateRenderTargetView(d3d_res, &rtv_desc, result->rtv.cpu_handle);
        create_succeed = true;
    }

    if (utils::has_flag(result->desc.textuer.desc.usages, Texture::UsageFlags::DSV)) {
        m_Logger->warn("Unimplement for {}",
                       fmt::styled(magic_enum::enum_name(Texture::UsageFlags::DSV), fmt::fg(fmt::color::red)));
    }

    if (!create_succeed) {
        m_Logger->warn("create failed since the texture usages() is not for TextureView",
                       fmt::styled(magic_enum::enum_flags_name(result->desc.textuer.desc.usages), fmt::fg(fmt::color::red)));
        return nullptr;
    }

    return result;
}

auto DX12Device::CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler> {
    D3D12_FILTER filter = D3D12_ENCODE_BASIC_FILTER(
        to_d3d_filter_type(desc.min_filter),
        to_d3d_filter_type(desc.mag_filter),
        to_d3d_filter_type(desc.mipmap_filter),
        // TODO: Support comaprison filter
        D3D12_FILTER_REDUCTION_TYPE_STANDARD);

    D3D12_SAMPLER_DESC d3d_desc{
        .Filter         = filter,
        .AddressU       = to_d3d_address_mode(desc.address_u),
        .AddressV       = to_d3d_address_mode(desc.address_v),
        .AddressW       = to_d3d_address_mode(desc.address_w),
        .MipLODBias     = 0,
        .MaxAnisotropy  = desc.max_anisotropy,
        .ComparisonFunc = to_d3d_compare_function(desc.compare),
        .BorderColor    = {0.0f, 0.0f, 0.0f, 0.0f},
        .MinLOD         = desc.min_lod,
        .MaxLOD         = desc.max_load,
    };

    auto result = std::make_shared<DX12DescriptorWrapper<Sampler>>(desc);

    result->sampler = m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->Allocate();
    m_Device->CreateSampler(&d3d_desc, result->sampler.cpu_handle);

    return result;
}

auto DX12Device::CompileShader(Shader::Desc desc) -> std::shared_ptr<Shader> {
    std::pmr::vector<std::pmr::wstring> args;

    // shader name
    args.emplace_back(std::pmr::wstring(desc.name.begin(), desc.name.end()));

    // shader entry
    args.emplace_back(L"-E");
    args.emplace_back(desc.entry.begin(), desc.entry.end());

    // shader model
    args.emplace_back(L"-T");
    args.emplace_back(to_shader_model_compile_flag(desc.type, m_FeatureSupport.HighestShaderModel()));

    // We use row major order
    args.emplace_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);

    // make sure all resource bound
    args.emplace_back(DXC_ARG_ALL_RESOURCES_BOUND);
#ifdef _DEBUG
    args.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL0);
    args.emplace_back(L"-Zs");  // dynamic shader editing
#endif
    std::pmr::vector<const wchar_t*> p_args;
    std::transform(args.begin(), args.end(), std::back_insert_iterator(p_args), [](const auto& arg) { return arg.data(); });

    std::pmr::string compile_command;
    for (std::pmr::wstring arg : args) {
        compile_command.append(arg.begin(), arg.end());
        compile_command.push_back(' ');
    }
    m_Logger->debug("Compile Shader: {}", compile_command);

    ComPtr<IDxcIncludeHandler> include_handler;

    m_Utils->CreateDefaultIncludeHandler(&include_handler);
    DxcBuffer source_buffer{
        .Ptr      = desc.source_code.data(),
        .Size     = desc.source_code.size(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<IDxcResult> compile_result;

    ThrowIfFailed(m_ShaderCompiler->Compile(
        &source_buffer,
        p_args.data(),
        p_args.size(),
        include_handler.Get(),
        IID_PPV_ARGS(&compile_result)));

    ComPtr<IDxcBlobUtf8> compile_errors;
    compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compile_errors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (compile_errors != nullptr && compile_errors->GetStringLength() != 0) {
        m_Logger->warn("Warnings and Errors:\n{}\n", compile_errors->GetStringPointer());
    }

    HRESULT hr;
    compile_result->GetStatus(&hr);
    if (FAILED(hr)) {
        m_Logger->error("compilation Failed\n");
        return nullptr;
    }

    ComPtr<IDxcBlob>      shader_buffer;
    ComPtr<IDxcBlobUtf16> shader_name;

    compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_buffer), &shader_name);
    if (shader_buffer == nullptr) {
        return nullptr;
    }
    return std::make_shared<Shader>(desc, core::Buffer{shader_buffer->GetBufferSize(), reinterpret_cast<const std::byte*>(shader_buffer->GetBufferPointer())});
}

auto DX12Device::CreateRenderPipeline(RenderPipeline::Desc desc) -> std::shared_ptr<RenderPipeline> {
    auto result = std::make_shared<DX12RenderPipeline>(std::move(desc));

    if (result->desc.bindless) {
    } else {
        // try to deserializer root signature from shader code
        {
            for (const auto& shader : {result->desc.vs, result->desc.ps, result->desc.gs}) {
                if (shader == nullptr || shader->binary_data.Empty()) continue;

                HRESULT hr = D3D12CreateVersionedRootSignatureDeserializer(
                    shader->binary_data.GetData(),
                    shader->binary_data.GetDataSize(),
                    IID_PPV_ARGS(&result->root_signature_deserializer));

                if (SUCCEEDED(hr)) {
                    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* p_desc = nullptr;
                    result->root_signature_deserializer->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &p_desc);
                    assert(p_desc->Version == D3D_ROOT_SIGNATURE_VERSION_1_1);
                    result->root_signature_desc = p_desc->Desc_1_1;

                    if (SUCCEEDED(m_Device->CreateRootSignature(
                            0,
                            shader->binary_data.GetData(),
                            shader->binary_data.GetDataSize(),
                            IID_PPV_ARGS(&result->root_signature)))) {
                        break;
                    }
                }
            }

            if (result->root_signature == nullptr) {
                m_Logger->warn("Faild create root signature from shader. May you create root signature in shader? Pipeline Name: {}",
                               fmt::styled(result->desc.name, fmt::fg(fmt::color::red)));
                return nullptr;
            }
        }
    }

    // Try to create pipeline state object
    {
        auto input_layout = to_d3d_input_layout_desc(result->desc.input_layout);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d_desc{
            .pRootSignature = result->root_signature.Get(),

            .VS = {
                .pShaderBytecode = result->desc.vs ? result->desc.vs->binary_data.GetData() : nullptr,
                .BytecodeLength  = result->desc.vs ? result->desc.vs->binary_data.GetDataSize() : 0,
            },
            .PS = {
                .pShaderBytecode = result->desc.ps ? result->desc.ps->binary_data.GetData() : nullptr,
                .BytecodeLength  = result->desc.ps ? result->desc.ps->binary_data.GetDataSize() : 0,
            },
            .GS = {
                .pShaderBytecode = result->desc.gs ? result->desc.gs->binary_data.GetData() : nullptr,
                .BytecodeLength  = result->desc.gs ? result->desc.gs->binary_data.GetDataSize() : 0,
            },
            .BlendState            = to_d3d_blend_desc(result->desc.blend_config),
            .SampleMask            = UINT_MAX,
            .RasterizerState       = to_d3d_rasterizer_desc(result->desc.rasterizer_config),
            .InputLayout           = input_layout.desc,
            .PrimitiveTopologyType = to_d3d_primitive_topology_type(result->desc.topology),
            .NumRenderTargets      = 1,
            .RTVFormats            = {to_dxgi_format(result->desc.render_format)},
            .DSVFormat             = to_dxgi_format(result->desc.depth_sentcil_format),
            .SampleDesc            = {.Count = 1, .Quality = 0},
            .NodeMask              = 0,
        };

        HRESULT hr = m_Device->CreateGraphicsPipelineState(&d3d_desc, IID_PPV_ARGS(&result->pso));
        if (FAILED(hr)) {
            m_Logger->warn("Failed to create pipeline. Name: {}", result->desc.name);
            return nullptr;
        }

        result->pso->SetName(std::pmr::wstring(result->desc.name.begin(), result->desc.name.end()).data());
    }

    return result;
}

auto DX12Device::RequestDynamicDescriptors(std::size_t num, D3D12_DESCRIPTOR_HEAP_TYPE type) -> Descriptor {
    if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
        return m_GpuDescriptorAllocators[0]->Allocate(num);
    } else if (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        return m_GpuDescriptorAllocators[1]->Allocate(num);
    } else {
        auto err_msg = fmt::format("Can not create a shader visiable {} descriptor", magic_enum::enum_name(type));
        m_Logger->error(err_msg);
        throw std::invalid_argument(err_msg.c_str());
    }
}

bool DX12Device::FenceFinished(std::uint64_t fence_value) {
    for (auto& queue : m_CommandQueues) {
        if (!queue->IsFenceComplete(m_ReiteredGpuDescriptors.front().second)) {
            return false;
        }
    }
    return true;
}

}  // namespace hitagi::gfx