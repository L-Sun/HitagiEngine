#include "dx12_resource.hpp"
#include "d3d12.h"
#include "dx12_command_list.hpp"
#include "dx12_device.hpp"
#include "utils.hpp"
#include <hitagi/utils/utils.hpp>

#include <fmt/color.h>
#include <spdlog/logger.h>

#include <algorithm>

namespace hitagi::gfx {
DX12GPUBuffer::DX12GPUBuffer(DX12Device& device, GPUBufferDesc desc, std::span<const std::byte> initial_data)
    : GPUBuffer(device, desc), buffer_size(m_Desc.element_size * m_Desc.element_count)

{
    const auto logger = device.GetLogger();

    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_FLAGS  flags         = D3D12_RESOURCE_FLAG_NONE;
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopySrc)) {
        initial_state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopyDst)) {
        initial_state |= D3D12_RESOURCE_STATE_COPY_DEST;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Index)) {
        initial_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if (m_Desc.element_size != sizeof(std::uint16_t) && m_Desc.element_size != sizeof(std::uint32_t)) {
            logger->warn("Index buffer element size must be 16 bits or 32 bits");
        }
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Vertex)) {
        initial_state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Constant)) {
        initial_state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        buffer_size = utils::align(m_Desc.element_size * m_Desc.element_count, 256);
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Storage)) {
        initial_state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12MA::ALLOCATION_DESC allocation_desc = {
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead)) {
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Storage)) {
            auto error_message = fmt::format(
                "GPU buffer({}) cannot be mapped and used as storage buffer at the same time",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Storage)) {
            auto error_message = fmt::format(
                "GPU buffer({}) cannot be mapped and used as storage buffer at the same time",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    }

    logger->trace("Create GPU buffer({}) with {} bytes", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)), buffer_size);

    auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size, flags);
    if (FAILED(device.GetAllocator()->CreateResource(
            &allocation_desc,
            &resource_desc,
            initial_state,
            nullptr,
            &allocation,
            IID_PPV_ARGS(&resource)))) {
        const auto error_message = fmt::format(
            "Failed to create GPU buffer({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    resource->SetName(std::wstring(m_Name.begin(), m_Name.end()).c_str());

    if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapRead) ||
        utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
        if (FAILED(resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped_ptr)))) {
            const auto error_message = fmt::format(
                "Failed to map GPU buffer({})",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
    }

    if (utils::has_flag(desc.usages, GPUBufferUsageFlags::Constant)) {
        cbvs                                     = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate(m_Desc.element_count);
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {
            .SizeInBytes = static_cast<UINT>(utils::align(m_Desc.element_size, 256)),
        };
        for (std::size_t i = 0; i < m_Desc.element_count; ++i) {
            cbv_desc.BufferLocation = resource->GetGPUVirtualAddress() + i * cbv_desc.SizeInBytes;
            device.GetDevice()->CreateConstantBufferView(
                &cbv_desc,
                CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvs.cpu_handle, i, cbvs.increment_size));
        }
    }
    if (utils::has_flag(desc.usages, GPUBufferUsageFlags::Storage)) {
        uavs                                      = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate(m_Desc.element_count);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer        = {
                       .NumElements         = 1,
                       .StructureByteStride = static_cast<UINT>(m_Desc.element_size),
                       .Flags               = D3D12_BUFFER_UAV_FLAG_NONE,
            },
        };
        for (std::size_t i = 0; i < m_Desc.element_count; ++i) {
            uav_desc.Buffer.FirstElement = i;
            device.GetDevice()->CreateUnorderedAccessView(
                resource.Get(), nullptr,
                &uav_desc,
                CD3DX12_CPU_DESCRIPTOR_HANDLE(uavs.cpu_handle, i, uavs.increment_size));
        }
    }

    if (!initial_data.empty()) {
        if (initial_data.size() > m_Desc.element_size * m_Desc.element_count) {
            logger->warn(
                "the initial data size({}) is larger than gpu buffer({}) size({} x {}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.element_size, fmt::fg(fmt::color::green)),
                fmt::styled(m_Desc.element_count, fmt::fg(fmt::color::green)),
                fmt::styled(desc.name, fmt::fg(fmt::color::green)));
        }

        if (mapped_ptr && utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
            std::memcpy(mapped_ptr, initial_data.data(), std::min(initial_data.size(), m_Desc.element_size * m_Desc.element_count));
        } else if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopyDst)) {
            auto upload_buffer = DX12GPUBuffer(
                device,
                {
                    .name         = fmt::format("UploadBuffer-({})", m_Desc.name),
                    .element_size = std::min(initial_data.size(), m_Desc.element_size * m_Desc.element_count),
                    .usages       = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
                },
                {initial_data.data(), std::min(initial_data.size(), m_Desc.element_size * m_Desc.element_count)});

            auto copy_context = device.CreateCopyContext("CreateGPUBuffer");
            copy_context->Begin();
            copy_context->CopyBuffer(upload_buffer, 0, *this, 0, upload_buffer.buffer_size);
            copy_context->End();

            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);
            copy_queue.Submit({copy_context.get()});
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "Can not initialize gpu buffer({}) using upload heap without the flag {} or {}, the actual flags are {}",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)),
                fmt::styled(magic_enum::enum_name(GPUBufferUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(magic_enum::enum_name(GPUBufferUsageFlags::MapWrite), fmt::fg(fmt::color::green)),
                fmt::styled(magic_enum::enum_flags_name(m_Desc.usages), fmt::fg(fmt::color::red)));

            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

auto DX12GPUBuffer::GetMappedPtr() const noexcept -> std::byte* {
    return mapped_ptr;
}

DX12Texture::DX12Texture(DX12Device& device, TextureDesc desc, std::span<const std::byte> initial_data) : Texture(device, desc) {
    const auto logger = device.GetLogger();

    logger->trace("Create texture({})", fmt::styled(desc.name, fmt::fg(fmt::color::green)));

    if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.array_size == 0) {
        const auto error_message = fmt::format(
            "Can not create zero size texture, Name: {}, the actual size is ({} x {} x {})[{}]",
            fmt::styled(desc.name, fmt::fg(fmt::color::red)),
            fmt::styled(desc.width, fmt::fg(fmt::color::red)),
            fmt::styled(desc.height, fmt::fg(fmt::color::red)),
            fmt::styled(desc.depth, fmt::fg(fmt::color::red)),
            fmt::styled(desc.array_size, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    D3D12_RESOURCE_FLAGS resource_flag = D3D12_RESOURCE_FLAG_NONE;
    if (utils::has_flag(desc.usages, TextureUsageFlags::UAV)) {
        resource_flag |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (utils::has_flag(desc.usages, TextureUsageFlags::RTV)) {
        resource_flag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (utils::has_flag(desc.usages, TextureUsageFlags::DSV)) {
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
            1,
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

    std::optional<D3D12_CLEAR_VALUE> optimized_clear_value{};
    if (desc.clear_value.has_value() && (utils::has_flag(desc.usages, TextureUsageFlags::DSV) || utils::has_flag(desc.usages, TextureUsageFlags::RTV))) {
        optimized_clear_value = to_d3d_clear_value(desc.clear_value.value(), desc.format);
        if (optimized_clear_value->Format == DXGI_FORMAT_R16_TYPELESS) {
            optimized_clear_value->Format = DXGI_FORMAT_D16_UNORM;
        } else if (optimized_clear_value->Format == DXGI_FORMAT_R32_TYPELESS) {
            optimized_clear_value->Format = DXGI_FORMAT_D32_FLOAT;
        } else if (optimized_clear_value->Format == DXGI_FORMAT_R32G8X24_TYPELESS) {
            optimized_clear_value->Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        }
    }

    D3D12MA::ALLOCATION_DESC allocation_desc{
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };
    if (FAILED(device.GetAllocator()->CreateResource(
            &allocation_desc,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            optimized_clear_value.has_value() ? &optimized_clear_value.value() : nullptr,
            &allocation, IID_PPV_ARGS(&resource)))) {
        const auto error_message = fmt::format("Can not create texture({})", fmt::styled(desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    resource->SetName(std::pmr::wstring(desc.name.begin(), desc.name.end()).data());

    if (utils::has_flag(desc.usages, TextureUsageFlags::SRV)) {
        srv                 = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate();
        const auto srv_desc = to_d3d_srv_desc(m_Desc);
        device.GetDevice()->CreateShaderResourceView(resource.Get(), &srv_desc, srv.cpu_handle);
    }
    if (utils::has_flag(desc.usages, TextureUsageFlags::UAV)) {
        uav                 = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate();
        const auto uav_desc = to_d3d_uav_desc(m_Desc);
        device.GetDevice()->CreateUnorderedAccessView(resource.Get(), nullptr, &uav_desc, uav.cpu_handle);
    }

    if (utils::has_flag(m_Desc.usages, TextureUsageFlags::RTV)) {
        rtv                 = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate();
        const auto rtv_desc = to_d3d_rtv_desc(m_Desc);
        device.GetDevice()->CreateRenderTargetView(resource.Get(), &rtv_desc, rtv.cpu_handle);
    }
    if (utils::has_flag(m_Desc.usages, TextureUsageFlags::DSV)) {
        dsv                 = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).Allocate();
        const auto dsv_desc = to_d3d_dsv_desc(m_Desc);
        device.GetDevice()->CreateDepthStencilView(resource.Get(), &dsv_desc, dsv.cpu_handle);
    }

    if (!initial_data.empty()) {
        auto upload_buffer = DX12GPUBuffer(
            device,
            {
                .name         = "UploadBuffer",
                .element_size = GetRequiredIntermediateSize(resource.Get(), 0, resource_desc.Subresources(device.GetDevice().Get())),
                .usages       = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
            });

        D3D12_SUBRESOURCE_DATA textureData = {
            .pData      = initial_data.data(),
            .RowPitch   = static_cast<LONG_PTR>(desc.width * (get_format_bit_size(desc.format) >> 3)),
            .SlicePitch = textureData.RowPitch * desc.height,
        };

        auto copy_context = device.CreateCopyContext("UploadTexture");
        copy_context->Begin();
        UpdateSubresources(
            std::static_pointer_cast<DX12CopyCommandList>(copy_context)->command_list.Get(),
            resource.Get(),
            upload_buffer.resource.Get(),
            0,
            0,
            resource_desc.Subresources(device.GetDevice().Get()),
            &textureData);
        copy_context->End();

        auto& copy_queue = device.GetCommandQueue(CommandType::Copy);
        copy_queue.Submit({copy_context.get()});
        copy_queue.WaitIdle();
    }
}

DX12Texture::DX12Texture(DX12SwapChain& swap_chain, std::uint32_t index)
    : Texture(swap_chain.GetDevice(),
              TextureDesc{
                  .name        = fmt::format("{}-{}", swap_chain.GetName(), index),
                  .width       = swap_chain.GetWidth(),
                  .height      = swap_chain.GetHeight(),
                  .format      = swap_chain.GetFormat(),
                  .clear_value = swap_chain.GetDesc().clear_value,
                  .usages      = TextureUsageFlags::RTV | TextureUsageFlags::CopyDst,
              }) {
    const auto logger = m_Device.GetLogger();
    logger->trace("Create swap chain back buffer ({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    auto dx12_swap_chain = swap_chain.GetDX12SwapChain();
    if (FAILED(dx12_swap_chain->GetBuffer(index, IID_PPV_ARGS(&resource)))) {
        auto error_message = fmt::format(
            "Failed to get swap chain back buffer");
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    resource->SetName(std::pmr::wstring(m_Desc.name.begin(), m_Desc.name.end()).c_str());

    rtv = static_cast<DX12Device&>(m_Device).GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate();
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{
        .Format        = to_dxgi_format(m_Desc.format),
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
        .Texture2D     = {
                .MipSlice   = 0,
                .PlaneSlice = 0,
        },
    };
    static_cast<DX12Device&>(m_Device).GetDevice()->CreateRenderTargetView(resource.Get(), &rtv_desc, rtv.cpu_handle);
}

DX12Sampler::DX12Sampler(DX12Device& device, SamplerDesc desc) : Sampler(device, desc) {
    const auto logger = device.GetLogger();
    logger->trace("Create sampler ({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    const D3D12_SAMPLER_DESC sampler_desc{
        .Filter = D3D12_ENCODE_BASIC_FILTER(
            to_d3d_filter_type(desc.min_filter),
            to_d3d_filter_type(desc.mag_filter),
            to_d3d_filter_type(desc.mipmap_filter),
            // TODO: Support comparison filter
            D3D12_FILTER_REDUCTION_TYPE_STANDARD),
        .AddressU       = to_d3d_address_mode(desc.address_u),
        .AddressV       = to_d3d_address_mode(desc.address_v),
        .AddressW       = to_d3d_address_mode(desc.address_w),
        .MaxAnisotropy  = static_cast<UINT>(desc.max_anisotropy),
        .ComparisonFunc = to_d3d_compare_function(desc.compare_op),
        .MinLOD         = desc.min_lod,
        .MaxLOD         = desc.max_lod,
    };
    sampler = device.GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).Allocate();
    device.GetDevice()->CreateSampler(&sampler_desc, sampler.cpu_handle);
}

DX12Shader::DX12Shader(DX12Device& device, ShaderDesc desc, std::span<const std::byte> _binary_program) : Shader(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create shader ({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    if (_binary_program.empty()) {
        binary_program = device.GetShaderCompiler().CompileToDXIL(m_Desc);
        if (binary_program.Empty()) {
            auto error_message = fmt::format(
                "Failed to compile shader({})",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
    } else {
        binary_program = _binary_program;
    }
}

DX12RenderPipeline::DX12RenderPipeline(DX12Device& device, RenderPipelineDesc desc) : RenderPipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create render pipeline ({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    D3D12_SHADER_BYTECODE vs{}, ps{}, gs{};
    for (const auto& _shader : m_Desc.shaders) {
        if (auto shader = _shader.lock(); shader != nullptr) {
            auto dx12_shader = std::static_pointer_cast<DX12Shader>(shader);
            switch (shader->GetDesc().type) {
                case ShaderType::Vertex:
                    vs = dx12_shader->GetShaderByteCode();
                    break;
                case ShaderType::Pixel:
                    ps = dx12_shader->GetShaderByteCode();
                    break;
                case ShaderType::Geometry:
                    gs = dx12_shader->GetShaderByteCode();
                    break;
                case ShaderType::Compute: {
                    auto error_message = fmt::format(
                        "Compute shader is not supported in render pipeline({})",
                        fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
                    logger->error(error_message);
                    throw std::runtime_error(error_message);
                }
            }
        }
    }

    const auto d3d_input_layout = to_d3d_input_layout(m_Desc.vertex_input_layout);

    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{
        .pRootSignature        = static_cast<DX12BindlessUtils&>(device.GetBindlessUtils()).GetBindlessRootSignature().Get(),
        .VS                    = vs,
        .PS                    = ps,
        .GS                    = gs,
        .BlendState            = to_d3d_blend_desc(m_Desc.blend_state),
        .SampleMask            = UINT_MAX,
        .RasterizerState       = to_d3d_rasterizer_state(m_Desc.rasterization_state),
        .DepthStencilState     = to_d3d_depth_stencil_state(m_Desc.depth_stencil_state),
        .InputLayout           = d3d_input_layout.desc,
        .PrimitiveTopologyType = to_d3d_primitive_topology_type(m_Desc.assembly_state.primitive),
        .NumRenderTargets      = 1,
        .RTVFormats            = {
            to_dxgi_format(m_Desc.render_format),
        },
        .DSVFormat  = to_dxgi_format(m_Desc.depth_stencil_format),
        .SampleDesc = {.Count = 1, .Quality = 0},
        .NodeMask   = 0,
    };

    if (FAILED(device.GetDevice()->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)))) {
        const auto error_message = fmt::format(
            "Failed to create graphics pipeline state({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline->SetName(std::pmr::wstring(m_Name.begin(), m_Name.end()).c_str());
}

DX12ComputePipeline::DX12ComputePipeline(DX12Device& device, ComputePipelineDesc desc) : ComputePipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create compute pipeline ({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    const auto root_signature = static_cast<DX12BindlessUtils&>(device.GetBindlessUtils()).GetBindlessRootSignature().Get();

    const auto dx12_shader = std::static_pointer_cast<DX12Shader>(m_Desc.cs.lock());
    if (dx12_shader == nullptr) {
        const auto error_message = fmt::format(
            "Compute shader is not specified in compute pipeline({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    } else if (dx12_shader->GetDesc().type != ShaderType::Compute) {
        const auto error_message = fmt::format(
            "Shader({}) type is not compute in compute pipeline({})",
            fmt::styled(dx12_shader->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    const D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{
        .pRootSignature = root_signature,
        .CS             = dx12_shader->GetShaderByteCode(),
        .NodeMask       = 0,
    };

    if (FAILED(device.GetDevice()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)))) {
        const auto error_message = fmt::format(
            "Failed to create compute pipeline state({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline->SetName(std::pmr::wstring(m_Name.begin(), m_Name.end()).c_str());
}

DX12SwapChain::DX12SwapChain(DX12Device& device, SwapChainDesc desc) : SwapChain(device, desc) {
    const auto logger = device.GetLogger();
    logger->trace("Create swap chain ({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    if (desc.window.ptr == nullptr) {
        auto error_message = fmt::format(
            "Failed to create swap chain because the window.ptr is {}",
            fmt::styled("nullptr", fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }
    const HWND h_wnd = static_cast<HWND>(desc.window.ptr);
    if (!IsWindow(h_wnd)) {
        auto error_message = fmt::format(
            "The window ptr({}) is not a valid window.",
            fmt::styled("nullptr", fmt::fg(fmt::color::red)));
        logger->error("The window ptr({}) is not a valid window.", desc.window.ptr);
        throw std::invalid_argument(error_message);
    }

    const auto factory = device.GetFactory();

    UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    if (desc.vsync) {
        BOOL allow_tearing = false;
        factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
        if (allow_tearing) {
            flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }
    }

    m_D3D12Desc = {
        .Format     = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo     = false,
        .SampleDesc = {
            .Count   = desc.sample_count,
            .Quality = 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling     = DXGI_SCALING_STRETCH,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags       = flags,
    };

    auto& gfx_queue = static_cast<DX12CommandQueue&>(device.GetCommandQueue(CommandType::Graphics));

    ComPtr<IDXGISwapChain1> p_swap_chain;
    if (FAILED(factory->CreateSwapChainForHwnd(gfx_queue.GetDX12Queue().Get(), h_wnd, &m_D3D12Desc, nullptr, nullptr, &p_swap_chain))) {
        const auto error_message = fmt::format(
            "Failed to create swap chain ({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    if (FAILED(p_swap_chain.As(&m_SwapChain))) {
        const auto error_message = fmt::format(
            "Failed to create swap chain ({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    if (FAILED(factory->MakeWindowAssociation(h_wnd, DXGI_MWA_NO_ALT_ENTER))) {
        const auto error_message = fmt::format(
            "Failed to make window association with swap chain({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    m_SwapChain->GetDesc1(&m_D3D12Desc);

    logger->trace("retrieval back buffers");
    for (std::size_t index = 0; index < m_D3D12Desc.BufferCount; index++) {
        m_BackBuffers.emplace_back(*this, index);
    }
}

auto DX12SwapChain::GetFormat() const noexcept -> Format {
    return from_dxgi_format(m_D3D12Desc.Format);
}

void DX12SwapChain::Present() {
    auto& queue = static_cast<DX12CommandQueue&>(m_Device.GetCommandQueue(CommandType::Graphics));
    if (m_D3D12Desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) {
        m_SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    } else {
        m_SwapChain->Present(m_Desc.vsync ? 1 : 0, 0);
    }
    // Workaround for inserting a fence present
    queue.Submit({});
}

void DX12SwapChain::Resize() {
    m_Device.GetCommandQueue(CommandType::Graphics).WaitIdle();
    m_BackBuffers.clear();

    const HWND h_wnd = static_cast<HWND>(m_Desc.window.ptr);
    RECT       rect;
    GetClientRect(h_wnd, &rect);

    const auto width  = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;

    if (FAILED(m_SwapChain->ResizeBuffers(
            m_D3D12Desc.BufferCount,
            width,
            height,
            m_D3D12Desc.Format,
            m_D3D12Desc.Flags))) {
        const auto error_message = fmt::format(
            "Failed to resize swap chain");
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    m_SwapChain->GetDesc1(&m_D3D12Desc);
    for (std::size_t index = 0; index < m_D3D12Desc.BufferCount; index++) {
        m_BackBuffers.emplace_back(*this, index);
    }
}

auto DX12SwapChain::AcquireTextureForRendering() -> DX12Texture& {
    return m_BackBuffers[m_SwapChain->GetCurrentBackBufferIndex()];
}

}  // namespace hitagi::gfx