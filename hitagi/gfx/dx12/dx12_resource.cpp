module;
#include <d3d12.h>
#include <wrl.h>
#include <D3D12MemAlloc.h>
#include <dxgi1_6.h>
#include <d3d12shader.h>
#include <fmt/color.h>
#include <spdlog/logger.h>
#include <d3dx12/d3dx12.h>

export module gfx.dx12:resource;
import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :bindless;
import :utils;
import :descriptor_heap;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

struct DX12GPUBuffer : public GPUBuffer {
    DX12GPUBuffer(D3D12MA::Allocator& allocator, std::shared_ptr<spdlog::logger> logger, GPUBufferDesc desc, std::span<const std::byte> initial_data = {});

    auto GetAllocationSize() const noexcept -> std::uint64_t final { return allocation ? allocation->GetSize() : Size(); }
    auto Map() -> std::byte* final;
    void UnMap() final;

    ComPtr<D3D12MA::Allocation> allocation;
    ComPtr<ID3D12Resource>      resource;

    std::shared_ptr<spdlog::logger> m_Logger;
    std::mutex                      map_mutex;
    std::uint16_t                   mapped_count{0};
};

struct DX12GPUBufferView final : public GPUBufferView {
    DX12GPUBufferView(DX12BindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc);
};

struct DX12Texture : public Texture {
    DX12Texture(D3D12MA::Allocator& allocator, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data = {});
    DX12Texture(DX12SwapChain& swap_chain, const std::shared_ptr<spdlog::logger>& logger, std::uint32_t index);
    DX12Texture(DX12Texture&&) = default;

    auto GetAllocationSize() const noexcept -> std::uint64_t final { return allocation ? allocation->GetSize() : 0; }

    ComPtr<D3D12MA::Allocation> allocation;
    ComPtr<ID3D12Resource>      resource;
};

struct DX12TextureView final : public TextureView {
    DX12TextureView(ID3D12Device& device, DX12BindlessUtils& bindings, DescriptorAllocator& rtv_allocator, DescriptorAllocator& dsv_allocator, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc);

    Descriptor rtv, dsv;
};

struct DX12Sampler : public Sampler {
    DX12Sampler(DX12BindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, SamplerDesc desc);
};

struct DX12Shader : public Shader {
    DX12Shader(const ShaderCompiler& compiler, const std::shared_ptr<spdlog::logger>& logger, ShaderDesc desc);

    inline auto GetDXILData() const noexcept -> std::span<const std::byte> final {
        return binary_program.Span<const std::byte>();
    }
    inline auto GetShaderByteCode() const noexcept -> D3D12_SHADER_BYTECODE {
        return {.pShaderBytecode = binary_program.GetData(), .BytecodeLength = binary_program.GetDataSize()};
    }

    core::Buffer binary_program;
};

struct DX12RenderPipeline : public RenderPipeline {
    DX12RenderPipeline(ID3D12Device& device, ID3D12RootSignature& root_signature, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders);

    ComPtr<ID3D12PipelineState> pipeline;
};

struct DX12ComputePipeline : public ComputePipeline {
    DX12ComputePipeline(ID3D12Device& device, ID3D12RootSignature& root_signature, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs);

    ComPtr<ID3D12PipelineState> pipeline;
};

class DX12SwapChain final : public SwapChain {
public:
    DX12SwapChain(const ComPtr<IDXGIFactory2>& factory, ID3D12CommandQueue& native_queue, CommandQueue& queue, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc);

    inline auto GetWidth() const noexcept -> std::uint32_t final { return m_D3D12Desc.Width; }
    inline auto GetHeight() const noexcept -> std::uint32_t final { return m_D3D12Desc.Height; }
    auto        GetFormat() const noexcept -> Format final;

    void Present() final;
    void Resize() final;

    auto AcquireTextureForRendering() -> utils::optional_ref<Texture> final;

    inline auto GetDX12SwapChain() const noexcept { return m_SwapChain; }

private:
    CommandQueue&                   m_Queue;
    std::shared_ptr<spdlog::logger> m_Logger;
    ComPtr<IDXGISwapChain4>         m_SwapChain;
    math::vec2u                     m_Size;
    DXGI_SWAP_CHAIN_DESC1           m_D3D12Desc;

    std::pmr::vector<DX12Texture> m_BackBuffers;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

DX12GPUBuffer::DX12GPUBuffer(D3D12MA::Allocator& allocator, std::shared_ptr<spdlog::logger> logger, GPUBufferDesc desc, std::span<const std::byte> initial_data) : GPUBuffer(std::move(desc)), m_Logger(std::move(logger)) {
    logger = m_Logger;

    if (Size() == 0) {
        const auto error_message = fmt::format(
            "GPU buffer({}) size must be larger than 0",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::StorageWrite)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12MA::ALLOCATION_DESC allocation_desc = {
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead)) {
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::StorageWrite)) {
            auto error_message = fmt::format(
                "GPU buffer({}) cannot be mapped and used as storage buffer at the same time",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
    }
    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::StorageWrite)) {
            auto error_message = fmt::format(
                "GPU buffer({}) cannot be mapped and used as storage buffer at the same time",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    }
    // Use GPU_UPLOAD heap for buffers with initial data + CopyDst (direct VRAM write via ReBAR)
    if (!initial_data.empty() &&
        utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopyDst) &&
        !utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite) &&
        !utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead)) {
        allocation_desc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD;
    }

    logger->trace("Create GPU buffer({}) with {} bytes", fmt::styled(GetName(), fmt::fg(fmt::color::green)), Size());

    auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(Size(), flags);
    if (FAILED(allocator.CreateResource(
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

        if (initial_data.size() > Size()) {
            logger->warn(
                "the initial data size({}) is larger than gpu buffer({}) size({}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                fmt::styled(GetName(), fmt::fg(fmt::color::green)),
                fmt::styled(Size(), fmt::fg(fmt::color::green)));
        }

        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
            auto mapped_ptr = Map();
            std::memcpy(mapped_ptr, initial_data.data(), std::min<std::size_t>(initial_data.size(), Size()));
            UnMap();
        } else if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopyDst)) {
            // Direct VRAM write via GPU_UPLOAD heap (ReBAR)
            std::byte* mapped_ptr = nullptr;
            if (FAILED(resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped_ptr)))) {
                const auto error_message = fmt::format(
                    "Failed to map GPU_UPLOAD buffer({})",
                    fmt::styled(GetName(), fmt::fg(fmt::color::green)));
                logger->error(error_message);
                throw std::runtime_error(error_message);
            }
            std::memcpy(mapped_ptr, initial_data.data(), std::min<std::size_t>(initial_data.size(), Size()));
            resource->Unmap(0, nullptr);
        } else {
            const auto error_message = fmt::format(
                "Can not initialize gpu buffer({}) using upload heap without the flag {} or {}, the actual flags are {}",
                fmt::styled(GetName(), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(GPUBufferUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(GPUBufferUsageFlags::MapWrite), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(m_Desc.usages), fmt::fg(fmt::color::red)));

            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

DX12GPUBufferView::DX12GPUBufferView(DX12BindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc) : GPUBufferView(bindings, storage_requirements, std::move(desc)) {
    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<DX12BindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(*static_cast<DX12GPUBuffer&>(*m_Desc.buffer).resource.Get(), m_Desc, Size());
}

auto DX12GPUBuffer::Map() -> std::byte* {
    const auto logger = m_Logger;
    if (!utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead) && !utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
        const auto error_message = fmt::format(
            "Can not map GPU buffer({}) without usage flag {} or {}",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)),
            fmt::styled(format_as(GPUBufferUsageFlags::MapRead), fmt::fg(fmt::color::green)),
            fmt::styled(format_as(GPUBufferUsageFlags::MapWrite), fmt::fg(fmt::color::green)));

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
    std::scoped_lock lock(map_mutex);
    mapped_count++;
    return mapped_ptr;
}

void DX12GPUBuffer::UnMap() {
    std::scoped_lock lock(map_mutex);

    if (mapped_count == 0) {
        const auto error_message = fmt::format(
            "Can not unmap GPU buffer({}) without map it!",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    mapped_count--;
    resource->Unmap(0, nullptr);
}

DX12Texture::DX12Texture(D3D12MA::Allocator& allocator, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data) : Texture(desc) {
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

    const auto format_bit_size   = get_format_bit_size(desc.format);
    const auto format_byte_size  = format_bit_size >> 3;
    const bool direct_cpu_upload = !initial_data.empty() && utils::has_flag(m_Desc.usages, TextureUsageFlags::CopyDst);

    D3D12MA::ALLOCATION_DESC allocation_desc{
        .HeapType = direct_cpu_upload ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_DEFAULT,
    };
    if (FAILED(allocator.CreateResource(
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
}

DX12Texture::DX12Texture(DX12SwapChain& swap_chain, const std::shared_ptr<spdlog::logger>& logger, std::uint32_t index)
    : Texture(
          TextureDesc{
              .name        = std::pmr::string(std::format("{}-{}", swap_chain.GetName(), index)),
              .width       = swap_chain.GetWidth(),
              .height      = swap_chain.GetHeight(),
              .format      = swap_chain.GetFormat(),
              .clear_value = swap_chain.GetDesc().clear_color,
              .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopyDst,
          }) {
    logger->trace("Create swap chain back buffer ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    auto dx12_swap_chain = swap_chain.GetDX12SwapChain();
    if (FAILED(dx12_swap_chain->GetBuffer(index, IID_PPV_ARGS(&resource)))) {
        auto error_message = fmt::format(
            "Failed to get swap chain back buffer");
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(resource.Get(), GetName());
}

DX12TextureView::DX12TextureView(ID3D12Device& device, DX12BindlessUtils& bindings, DescriptorAllocator& rtv_allocator, DescriptorAllocator& dsv_allocator, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc) : TextureView(bindings, std::move(desc)) {
    logger->trace("Create texture view({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    const auto fail = [&](std::string message) {
        const auto error_message = fmt::format(
            "Invalid texture view({}): {}",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)),
            message);
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    };

    if (!m_Desc.texture) {
        fail("texture is nullptr");
    }

    const auto& texture_desc = m_Desc.texture->GetDesc();
    const auto  single_subresource_view =
        m_Desc.type == TextureViewType::ShaderWrite ||
        m_Desc.type == TextureViewType::RenderTarget ||
        m_Desc.type == TextureViewType::DepthStencil;
    if (m_Desc.base_mip_level >= texture_desc.mip_levels) {
        fail("base mip level is outside the texture mip range");
    }
    if (m_Desc.mip_levels == 0) {
        m_Desc.mip_levels = single_subresource_view
                                ? 1
                                : texture_desc.mip_levels - m_Desc.base_mip_level;
    }
    if (m_Desc.base_mip_level + m_Desc.mip_levels > texture_desc.mip_levels) {
        fail("mip range exceeds texture mip levels");
    }
    if (single_subresource_view && m_Desc.mip_levels != 1) {
        fail("texture view type must reference exactly one mip level");
    }
    if (m_Desc.base_array_layer >= texture_desc.array_size) {
        fail("base array layer is outside the texture array range");
    }
    if (m_Desc.layer_count == 0) {
        m_Desc.layer_count = single_subresource_view
                                 ? 1
                                 : texture_desc.array_size - m_Desc.base_array_layer;
    }
    if (m_Desc.base_array_layer + m_Desc.layer_count > texture_desc.array_size) {
        fail("array layer range exceeds texture array size");
    }
    if (single_subresource_view && m_Desc.layer_count != 1) {
        fail("texture view type must reference exactly one array layer");
    }
    if (m_Desc.type == TextureViewType::ShaderRead && !utils::has_flag(texture_desc.usages, TextureUsageFlags::SRV)) {
        fail(std::format("shader read view requires texture usage {}", TextureUsageFlags::SRV));
    }
    if (m_Desc.type == TextureViewType::ShaderWrite && !utils::has_flag(texture_desc.usages, TextureUsageFlags::UAV)) {
        fail(std::format("shader write view requires texture usage {}", TextureUsageFlags::UAV));
    }
    if (m_Desc.type == TextureViewType::RenderTarget && !utils::has_flag(texture_desc.usages, TextureUsageFlags::RenderTarget)) {
        fail(std::format("render target view requires texture usage {}", TextureUsageFlags::RenderTarget));
    }
    if (m_Desc.type == TextureViewType::DepthStencil && !utils::has_flag(texture_desc.usages, TextureUsageFlags::DepthStencil)) {
        fail(std::format("depth stencil view requires texture usage {}", TextureUsageFlags::DepthStencil));
    }

    auto& dx12_texture = dynamic_cast<DX12Texture&>(*m_Desc.texture);
    if (m_Desc.type == TextureViewType::RenderTarget) {
        rtv                 = rtv_allocator.Allocate();
        const auto rtv_desc = to_d3d_rtv_desc(m_Desc);
        device.CreateRenderTargetView(dx12_texture.resource.Get(), &rtv_desc, rtv.GetCPUHandle());
    } else if (m_Desc.type == TextureViewType::DepthStencil) {
        dsv                 = dsv_allocator.Allocate();
        const auto dsv_desc = to_d3d_dsv_desc(m_Desc);
        device.CreateDepthStencilView(dx12_texture.resource.Get(), &dsv_desc, dsv.GetCPUHandle());
    }

    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<DX12BindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(*static_cast<DX12Texture&>(*m_Desc.texture).resource.Get(), m_Desc);
}

DX12Sampler::DX12Sampler(DX12BindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, SamplerDesc desc) : Sampler(bindings, std::move(desc)) {
    logger->trace("Create sampler ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<DX12BindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(m_Desc);
}

DX12Shader::DX12Shader(const ShaderCompiler& compiler, const std::shared_ptr<spdlog::logger>& logger, ShaderDesc desc) : Shader(std::move(desc)) {
    logger->trace("Create shader ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    binary_program = compiler.CompileToDXIL(m_Desc);
    if (binary_program.Empty()) {
        auto error_message = fmt::format(
            "Failed to compile shader({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
}

DX12RenderPipeline::DX12RenderPipeline(ID3D12Device& device, ID3D12RootSignature& root_signature, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) : RenderPipeline(std::move(desc)) {
    logger->trace("Create render pipeline ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

    D3D12_SHADER_BYTECODE vs{}, ps{}, gs{};
    for (const auto& shader : shaders) {
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

    const auto d3d_input_layout = to_d3d_input_layout(m_Desc.vertex_input_layout);

    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{
        .pRootSignature        = &root_signature,
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

    if (FAILED(device.CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)))) {
        const auto error_message = fmt::format(
            "Failed to create graphics pipeline state({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(pipeline.Get(), GetName());
}

DX12ComputePipeline::DX12ComputePipeline(ID3D12Device& device, ID3D12RootSignature& root_signature, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) : ComputePipeline(std::move(desc)) {
    logger->trace("Create compute pipeline ({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));


    const auto dx12_shader = std::static_pointer_cast<DX12Shader>(cs);
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
        .pRootSignature = &root_signature,
        .CS             = dx12_shader->GetShaderByteCode(),
        .NodeMask       = 0,
    };

    if (FAILED(device.CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)))) {
        const auto error_message = fmt::format(
            "Failed to create compute pipeline state({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    set_debug_name(pipeline.Get(), GetName());
}

DX12SwapChain::DX12SwapChain(const ComPtr<IDXGIFactory2>& factory, ID3D12CommandQueue& native_queue, CommandQueue& queue, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc) : SwapChain(desc), m_Queue(queue), m_Logger(std::move(logger)) {
    logger = m_Logger;
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


    ComPtr<IDXGISwapChain1> p_swap_chain;
    if (FAILED(factory->CreateSwapChainForHwnd(&native_queue, h_wnd, &m_D3D12Desc, nullptr, nullptr, &p_swap_chain))) {
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
        m_BackBuffers.emplace_back(*this, m_Logger, index);
    }
}

auto DX12SwapChain::GetFormat() const noexcept -> Format {
    return from_dxgi_format(m_D3D12Desc.Format);
}

void DX12SwapChain::Present() {
    auto& queue = m_Queue;
    if (m_D3D12Desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) {
        m_SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    } else {
        m_SwapChain->Present(m_Desc.vsync ? 1 : 0, 0);
    }
    // Workaround for inserting a fence present
    queue.Submit({});
}

void DX12SwapChain::Resize() {
    RECT rect;
    // unlike vulkan, we can keep the swap chain size unchanged when window is minimized
    if (!GetClientRect(static_cast<HWND>(m_Desc.window.ptr), &rect)) {
        return;
    }

    const auto width  = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;

    m_Queue.WaitIdle();
    m_BackBuffers.clear();

    if (FAILED(m_SwapChain->ResizeBuffers(
            m_D3D12Desc.BufferCount,
            width,
            height,
            m_D3D12Desc.Format,
            m_D3D12Desc.Flags))) {
        const auto error_message = fmt::format(
            "Failed to resize swap chain");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    m_SwapChain->GetDesc1(&m_D3D12Desc);

    for (std::size_t index = 0; index < m_D3D12Desc.BufferCount; index++) {
        m_BackBuffers.emplace_back(*this, m_Logger, index);
    }
}

auto DX12SwapChain::AcquireTextureForRendering() -> utils::optional_ref<Texture> {
    return m_BackBuffers[m_SwapChain->GetCurrentBackBufferIndex()];
}

}  // namespace hitagi::gfx
