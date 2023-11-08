#include "dx12_resource.hpp"
#include "dx12_command_list.hpp"
#include "dx12_device.hpp"
#include "dx12_utils.hpp"
#include <hitagi/utils/utils.hpp>

#include <d3d12shader.h>
#include <fmt/color.h>
#include <spdlog/logger.h>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/range/conversion.hpp>

#include <algorithm>

namespace hitagi::gfx {
DX12GPUBuffer::DX12GPUBuffer(DX12Device& device, GPUBufferDesc desc, std::span<const std::byte> initial_data) : GPUBuffer(device, std::move(desc)) {
    const auto logger = device.GetLogger();

    if (Size() == 0) {
        const auto error_message = fmt::format(
            "GPU buffer({}) size must be larger than 0",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Index)) {
        if (m_Desc.element_size != sizeof(std::uint16_t) && m_Desc.element_size != sizeof(std::uint32_t)) {
            const auto error_message = "Index buffer element size must be 16 bits or 32 bits";
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Constant)) {
        m_ElementAlignment = 256;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Storage)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12MA::ALLOCATION_DESC allocation_desc = {
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead)) {
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Storage)) {
            auto error_message = fmt::format(
                "GPU buffer({}) cannot be mapped and used as storage buffer at the same time",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Storage)) {
            auto error_message = fmt::format(
                "GPU buffer({}) cannot be mapped and used as storage buffer at the same time",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    }

    logger->trace("Create GPU buffer({}) with {} bytes", fmt::styled(GetName(), fmt::fg(fmt::color::green)), Size());

    auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(Size(), flags);
    if (FAILED(device.GetAllocator()->CreateResource(
            &allocation_desc,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            &allocation,
            IID_PPV_ARGS(&resource)))) {
        const auto error_message = fmt::format(
            "Failed to create GPU buffer({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    set_debug_name(resource.Get(), GetName());

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to buffer({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

        if (initial_data.size() % m_Desc.element_size != 0) {
            logger->warn(
                "the initial data size({}) is not a multiple of element size({}) of gpu buffer({}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.element_size, fmt::fg(fmt::color::green)),
                fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        }

        if (initial_data.size() > m_Desc.element_count * m_Desc.element_size) {
            logger->warn(
                "the element_count({}) in initial data is larger than the buffer element_count({}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size() / m_Desc.element_size, fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.element_count, fmt::fg(fmt::color::green)),
                fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        }

        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
            auto mapped_ptr = Map();
            if (m_ElementAlignment == 1 || m_ElementAlignment == m_Desc.element_size) {
                std::memcpy(mapped_ptr, initial_data.data(), std::min(initial_data.size(), Size()));
            } else {
                const std::size_t copy_count = std::min(initial_data.size() / m_Desc.element_size, m_Desc.element_count);
                // TODO parallel copy
                for (std::size_t i = 0; i < copy_count; i++) {
                    std::memcpy(
                        mapped_ptr + i * AlignedElementSize(),
                        initial_data.data() + i * m_Desc.element_size,
                        m_Desc.element_size);
                }
            }
            UnMap();
        } else if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopyDst)) {
            auto upload_buffer_usage_flags = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc;
            if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Constant)) {
                upload_buffer_usage_flags |= GPUBufferUsageFlags::Constant;  // for alignment
            }
            auto upload_buffer = DX12GPUBuffer(
                device,
                {
                    .name          = std::pmr::string(fmt::format("UploadBuffer-({})", GetName())),
                    .element_size  = m_Desc.element_size,
                    .element_count = m_Desc.element_count,
                    .usages        = upload_buffer_usage_flags,
                },
                {initial_data.data(), std::min(initial_data.size(), m_Desc.element_size * m_Desc.element_count)});

            auto copy_context = device.CreateCopyContext("CreateGPUBuffer");
            copy_context->Begin();
            copy_context->CopyBuffer(upload_buffer, 0, *this, 0, upload_buffer.Size());
            copy_context->End();

            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);
            copy_queue.Submit({{*copy_context}});
            copy_queue.WaitIdle();
        } else {
            const auto error_message = fmt::format(
                "Can not initialize gpu buffer({}) using upload heap without the flag {} or {}, the actual flags are {}",
                fmt::styled(GetName(), fmt::fg(fmt::color::green)),
                fmt::styled(GPUBufferUsageFlags::CopyDst, fmt::fg(fmt::color::green)),
                fmt::styled(GPUBufferUsageFlags::MapWrite, fmt::fg(fmt::color::green)),
                fmt::styled(m_Desc.usages, fmt::fg(fmt::color::red)));

            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

auto DX12GPUBuffer::Map() -> std::byte* {
    const auto logger = m_Device.GetLogger();
    if (!utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead) && !utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
        const auto error_message = fmt::format(
            "Can not map GPU buffer({}) without usage flag {} or {}",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)),
            fmt::styled(GPUBufferUsageFlags::MapRead, fmt::fg(fmt::color::green)),
            fmt::styled(GPUBufferUsageFlags::MapWrite, fmt::fg(fmt::color::green)));

        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    std::byte* mapped_ptr = nullptr;
    if (FAILED(resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped_ptr)))) {
        const auto error_message = fmt::format(
            "Failed to map GPU buffer({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    std::lock_guard lock(map_mutex);
    mapped_count++;
    return mapped_ptr;
}

void DX12GPUBuffer::UnMap() {
    std::lock_guard lock(map_mutex);

    if (mapped_count == 0) {
        const auto error_message = fmt::format(
            "Can not unmap GPU buffer({}) without map it!",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    mapped_count--;
    resource->Unmap(0, nullptr);
}

DX12Texture::DX12Texture(DX12Device& device, TextureDesc desc, std::span<const std::byte> initial_data) : Texture(device, desc) {
    const auto logger = device.GetLogger();

    logger->trace("Create texture({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.array_size == 0) {
        const auto error_message = fmt::format(
            "Can not create zero size texture, Name: {}, the actual size is ({} x {} x {})[{}]",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)),
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
    if (utils::has_flag(desc.usages, TextureUsageFlags::RenderTarget)) {
        resource_flag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (utils::has_flag(desc.usages, TextureUsageFlags::DepthStencil)) {
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
    if (desc.clear_value.has_value() && (utils::has_flag(desc.usages, TextureUsageFlags::DepthStencil) || utils::has_flag(desc.usages, TextureUsageFlags::RenderTarget))) {
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
            &allocation,
            IID_PPV_ARGS(&resource)))) {
        const auto error_message = fmt::format("Can not create texture({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(resource.Get(), GetName());

    if (utils::has_flag(m_Desc.usages, TextureUsageFlags::RenderTarget)) {
        rtv                 = device.GetRTVDescriptorAllocator().Allocate();
        const auto rtv_desc = to_d3d_rtv_desc(m_Desc);
        device.GetDevice()->CreateRenderTargetView(resource.Get(), &rtv_desc, rtv.GetCPUHandle());
    }
    if (utils::has_flag(m_Desc.usages, TextureUsageFlags::DepthStencil)) {
        dsv                 = device.GetDSVDescriptorAllocator().Allocate();
        const auto dsv_desc = to_d3d_dsv_desc(m_Desc);
        device.GetDevice()->CreateDepthStencilView(resource.Get(), &dsv_desc, dsv.GetCPUHandle());
    }

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to texture({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::CopyDst)) {
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
            copy_queue.Submit({{*copy_context}});
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "the texture({}) can not initialize with upload buffer without {}, the actual flags are {}",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)),
                fmt::styled(TextureUsageFlags::CopyDst, fmt::fg(fmt::color::green)),
                fmt::styled(desc.usages, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

DX12Texture::DX12Texture(DX12SwapChain& swap_chain, std::uint32_t index)
    : Texture(swap_chain.GetDevice(),
              TextureDesc{
                  .name        = std::pmr::string(fmt::format("{}-{}", swap_chain.GetName(), index)),
                  .width       = swap_chain.GetWidth(),
                  .height      = swap_chain.GetHeight(),
                  .format      = swap_chain.GetFormat(),
                  .clear_value = swap_chain.GetDesc().clear_color,
                  .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopyDst,
              }) {
    const auto logger = m_Device.GetLogger();
    logger->trace("Create swap chain back buffer ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    auto dx12_swap_chain = swap_chain.GetDX12SwapChain();
    if (FAILED(dx12_swap_chain->GetBuffer(index, IID_PPV_ARGS(&resource)))) {
        auto error_message = fmt::format(
            "Failed to get swap chain back buffer");
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(resource.Get(), GetName());

    rtv = static_cast<DX12Device&>(m_Device).GetRTVDescriptorAllocator().Allocate();
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{
        .Format        = to_dxgi_format(m_Desc.format),
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
        .Texture2D     = {
                .MipSlice   = 0,
                .PlaneSlice = 0,
        },
    };
    static_cast<DX12Device&>(m_Device).GetDevice()->CreateRenderTargetView(resource.Get(), &rtv_desc, rtv.GetCPUHandle());
}

DX12Sampler::DX12Sampler(DX12Device& device, SamplerDesc desc) : Sampler(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create sampler ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
}

DX12Shader::DX12Shader(DX12Device& device, ShaderDesc desc) : Shader(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create shader ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    binary_program = device.GetShaderCompiler().CompileToDXIL(m_Desc);
    if (binary_program.Empty()) {
        auto error_message = fmt::format(
            "Failed to compile shader({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
}

DX12RenderPipeline::DX12RenderPipeline(DX12Device& device, RenderPipelineDesc desc) : RenderPipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create render pipeline ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    D3D12_SHADER_BYTECODE vs{}, ps{}, gs{};
    for (const auto& _shader : m_Desc.shaders) {
        if (auto shader = _shader.lock(); shader != nullptr) {
            auto dx12_shader = std::dynamic_pointer_cast<DX12Shader>(shader);
            if (dx12_shader == nullptr) {
                auto error_message = fmt::format(
                    "Failed to cast shader({}) to DX12Shader",
                    fmt::styled(shader->GetName(), fmt::fg(fmt::color::green)));
                logger->error(error_message);
                throw std::runtime_error(error_message);
            }
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
                        fmt::styled(GetName(), fmt::fg(fmt::color::green)));
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
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(pipeline.Get(), GetName());
}

DX12ComputePipeline::DX12ComputePipeline(DX12Device& device, ComputePipelineDesc desc) : ComputePipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();
    logger->trace("Create compute pipeline ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    const auto root_signature = static_cast<DX12BindlessUtils&>(device.GetBindlessUtils()).GetBindlessRootSignature().Get();

    const auto dx12_shader = std::static_pointer_cast<DX12Shader>(m_Desc.cs.lock());
    if (dx12_shader == nullptr) {
        const auto error_message = fmt::format(
            "Compute shader is not specified in compute pipeline({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    } else if (dx12_shader->GetDesc().type != ShaderType::Compute) {
        const auto error_message = fmt::format(
            "Shader({}) type is not compute in compute pipeline({})",
            fmt::styled(dx12_shader->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
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
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(pipeline.Get(), GetName());
}

DX12SwapChain::DX12SwapChain(DX12Device& device, SwapChainDesc desc) : SwapChain(device, desc) {
    const auto logger = device.GetLogger();
    logger->trace("Create swap chain ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

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
        ComPtr<IDXGIFactory5> factory5;
        factory.As(&factory5);

        BOOL allow_tearing = false;
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
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
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    if (FAILED(p_swap_chain.As(&m_SwapChain))) {
        const auto error_message = fmt::format(
            "Failed to create swap chain ({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    if (FAILED(factory->MakeWindowAssociation(h_wnd, DXGI_MWA_NO_ALT_ENTER))) {
        const auto error_message = fmt::format(
            "Failed to make window association with swap chain({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
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

auto DX12SwapChain::AcquireTextureForRendering() -> Texture& {
    return m_BackBuffers[m_SwapChain->GetCurrentBackBufferIndex()];
}

}  // namespace hitagi::gfx