module;
#ifdef _WIN32
#include "interop/win32_macros.hpp"
#endif
module gfx.base;
#ifdef _WIN32
import interop.win32;
import interop.dx12;
import interop.d3d12ma;
#endif
import interop.fmt;
import interop.vulkan;
import interop.magic_enum;

import std;
import utils;
import core;
#ifdef _WIN32
import gfx.dx12;
#endif
import gfx.vulkan;
import gfx.mock;

namespace hitagi::gfx {

void validate_bindings(Device& device, const BindlessUtils& bindings) {
    bool compatible = false;
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            const auto* backend = dynamic_cast<const DX12BindlessUtils*>(&bindings);
            compatible          = backend && &backend->GetNativeDevice() == dynamic_cast<DX12Device&>(device).GetDevice().Get();
            break;
        }
#endif
        case Device::Type::Vulkan: {
            const auto* backend = dynamic_cast<const VulkanBindlessUtils*>(&bindings);
            compatible          = backend && *backend->GetNativeDevice() == *dynamic_cast<VulkanDevice&>(device).GetDevice();
            break;
        }
        case Device::Type::Mock: {
            const auto* backend = dynamic_cast<const MockBindlessUtils*>(&bindings);
            compatible          = backend && backend->IsOwner(&device);
            break;
        }
        default:
            break;
    }
    if (!compatible) throw std::invalid_argument("Binding facilities belong to another device");
}

void validate_queue(Device& device, const CommandQueue& queue, CommandType type) {
    bool compatible = queue.GetType() == type;
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            const auto*                          backend = dynamic_cast<const DX12CommandQueue*>(&queue);
            Microsoft::WRL::ComPtr<ID3D12Device> owner;
            compatible = compatible && backend && interop::succeeded(backend->GetDX12Queue()->GetDevice(IID_PPV_ARGS(&owner))) && owner.Get() == dynamic_cast<DX12Device&>(device).GetDevice().Get();
            break;
        }
#endif
        case Device::Type::Vulkan: {
            const auto* backend = dynamic_cast<const VulkanCommandQueue*>(&queue);
            compatible          = compatible && backend && backend->GetNativeDevice() == *dynamic_cast<VulkanDevice&>(device).GetDevice();
            break;
        }
        case Device::Type::Mock: {
            const auto* backend = dynamic_cast<const MockCommandQueue*>(&queue);
            compatible          = compatible && backend && backend->IsOwner(&device);
            break;
        }
        default:
            compatible = false;
    }
    if (!compatible) throw std::invalid_argument("Command queue belongs to another device or has the wrong command type");
}

#ifdef _WIN32
void initialize_dx12_texture(DX12Device& device, CommandQueues& queues, BindlessUtils& bindings, DX12Texture& texture, std::span<const std::byte> initial_data) {
    using Microsoft::WRL::ComPtr;
    const auto& desc              = texture.GetDesc();
    const auto  logger            = device.GetLogger();
    auto&       resource          = texture.resource;
    const auto  resource_desc     = CD3DX12_RESOURCE_DESC(resource->GetDesc());
    const auto  format_byte_size  = get_format_bit_size(desc.format) >> 3;
    const bool  direct_cpu_upload = !initial_data.empty() && utils::has_flag(desc.usages, TextureUsageFlags::CopyDst);
    // Initialize RT/DS resources to clear stale metadata from D3D12MA heap memory reuse.
    // Heaps with D3D12_HEAP_FLAG_CREATE_NOT_ZEROED may contain metadata from previously
    // placed RT/DS resources; DiscardResource clears this metadata to avoid undefined behavior.
    if (initial_data.empty() &&
        (utils::has_flag(desc.usages, TextureUsageFlags::RenderTarget) ||
         utils::has_flag(desc.usages, TextureUsageFlags::DepthStencil))) {
        const bool is_rt   = utils::has_flag(desc.usages, TextureUsageFlags::RenderTarget);
        auto       context = hitagi::gfx::GraphicsCommandContext::Create(device, queues, bindings, std::format("Init-{}", texture.GetName()));
        context->Begin();
        context->ResourceBarrier({}, {}, {{
                                             texture.Transition(is_rt ? BarrierAccess::RenderTarget : BarrierAccess::DepthStencilWrite, is_rt ? TextureLayout::RenderTarget : TextureLayout::DepthStencilWrite),
                                         }});
        static_cast<DX12GraphicsCommandList&>(*context).command_list->DiscardResource(resource.Get(), nullptr);
        context->ResourceBarrier({}, {}, {{
                                             texture.Transition(BarrierAccess::None, TextureLayout::Unkown),
                                         }});
        context->End();
        auto& gfx_queue = queues.Get(CommandType::Graphics);
        gfx_queue.Submit({{*context}});
        gfx_queue.WaitIdle();
    }

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to texture({})", fmt::styled(texture.GetName(), fmt::fg(fmt::color::green)));

        if (utils::has_flag(desc.usages, TextureUsageFlags::CopyDst)) {
            D3D12_SUBRESOURCE_DATA textureData = {
                .pData      = initial_data.data(),
                .RowPitch   = static_cast<LONG_PTR>(desc.width * format_byte_size),
                .SlicePitch = textureData.RowPitch * desc.height,
            };

            if (direct_cpu_upload) {
                if (interop::failed(resource->Map(0, nullptr, nullptr))) {
                    const auto error_message = fmt::format(
                        "Failed to map texture({}) for direct GPU upload heap initialization",
                        fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
                    logger->error(error_message);
                    throw std::runtime_error(error_message);
                }

                const auto result = resource->WriteToSubresource(
                    0,
                    nullptr,
                    initial_data.data(),
                    static_cast<UINT>(textureData.RowPitch),
                    static_cast<UINT>(textureData.SlicePitch));
                resource->Unmap(0, nullptr);

                if (interop::failed(result)) {
                    const auto error_message = fmt::format(
                        "Failed to initialize texture({}) via WriteToSubresource",
                        fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
                    logger->error(error_message);
                    throw std::runtime_error(error_message);
                }
            } else {
                const auto staging_size = GetRequiredIntermediateSize(resource.Get(), 0, resource_desc.Subresources(device.GetDevice().Get()));

                // GPU_UPLOAD staging buffer for VRAM→VRAM texture copy
                D3D12MA::ALLOCATION_DESC staging_alloc_desc{
                    .HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD,
                };
                auto                        staging_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(staging_size);
                ComPtr<D3D12MA::Allocation> staging_allocation;
                ComPtr<ID3D12Resource>      staging_resource;
                if (interop::failed(device.GetAllocator()->CreateResource(
                        &staging_alloc_desc,
                        &staging_resource_desc,
                        D3D12_RESOURCE_STATE_COMMON,
                        nullptr,
                        &staging_allocation,
                        IID_PPV_ARGS(&staging_resource)))) {
                    const auto error_message = fmt::format(
                        "Failed to create staging buffer for texture({})",
                        fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
                    logger->error(error_message);
                    throw std::runtime_error(error_message);
                }

                auto copy_context = hitagi::gfx::CopyCommandContext::Create(device, queues, "UploadTexture");
                copy_context->Begin();
                UpdateSubresources(
                    std::static_pointer_cast<DX12CopyCommandList>(copy_context)->command_list.Get(),
                    resource.Get(),
                    staging_resource.Get(),
                    0,
                    0,
                    resource_desc.Subresources(device.GetDevice().Get()),
                    &textureData);
                copy_context->End();

                auto& copy_queue = queues.Get(CommandType::Copy);
                copy_queue.Submit({{*copy_context}});
                copy_queue.WaitIdle();
            }
        } else {
            auto error_message = fmt::format(
                "the texture({}) can not initialize with upload buffer without {}, the actual flags are {}",
                fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)),
                fmt::styled(format_as(TextureUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(desc.usages), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}
#endif

void initialize_vulkan_texture(VulkanDevice& device, CommandQueues& queues, VulkanImage& texture, std::span<const std::byte> initial_data) {
    const auto& desc   = texture.GetDesc();
    const auto  logger = device.GetLogger();
    auto&       image  = texture.image;
    if (!initial_data.empty()) {
        logger->trace("Copy initial data to texture({})", fmt::styled(texture.GetName(), fmt::fg(fmt::color::green)));
        if (utils::has_flag(desc.usages, TextureUsageFlags::CopyDst)) {
            // Transition image to General layout for host image copy
            auto  context    = hitagi::gfx::CopyCommandContext::Create(device, queues, "HostImageCopy-Transition");
            auto& copy_queue = queues.Get(CommandType::Copy);

            context->Begin();
            context->ResourceBarrier(
                {}, {},
                {{
                    texture.Transition(BarrierAccess::CopyDst, TextureLayout::Common, PipelineStage::Copy),
                }});
            context->End();
            copy_queue.Submit({{*context}});
            copy_queue.WaitIdle();

            // Host-side copy via VK_EXT_host_image_copy
            const vk::MemoryToImageCopyEXT region{
                .pHostPointer      = initial_data.data(),
                .memoryRowLength   = 0,
                .memoryImageHeight = 0,
                .imageSubresource  = {
                    .aspectMask     = get_vk_image_aspect(desc),
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = desc.array_size,
                },
                .imageOffset = {.x = 0, .y = 0, .z = 0},
                .imageExtent = {.width = desc.width, .height = desc.height, .depth = desc.depth},
            };

            device.GetDevice().copyMemoryToImageEXT(vk::CopyMemoryToImageInfoEXT{
                .dstImage       = **image,
                .dstImageLayout = vk::ImageLayout::eGeneral,
                .regionCount    = 1,
                .pRegions       = &region,
            });
        } else {
            auto error_message = fmt::format(
                "the texture({}) can not initialize with staging buffer without {}, the actual flags are {}",
                fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)),
                fmt::styled(format_as(TextureUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(desc.usages), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

auto GPUBuffer::GetStorageViewRequirements(const Device& device) noexcept -> StorageViewRequirements {
    return device.GetStorageBufferViewRequirements();
}

auto Fence::Create(Device& device, std::uint64_t initial_value, std::string_view name) -> std::shared_ptr<Fence> {
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12Fence>(*backend.GetDevice().Get(), backend.GetLogger(), initial_value, name);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanTimelineSemaphore>(backend.GetDevice(), backend.GetCustomAllocator(), initial_value, name);
        }
        case Device::Type::Mock: {
            return std::make_shared<MockFence>(name);
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto CommandContext::Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, CommandType type, std::string_view name) -> std::shared_ptr<CommandContext> {
    validate_queue(device, queues.Get(type), type);
    validate_bindings(device, bindings);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            switch (type) {
                case CommandType::Graphics:
                    return std::make_shared<DX12GraphicsCommandList>(*backend.GetDevice().Get(), backend.GetLogger(), static_cast<DX12BindlessUtils&>(bindings), static_cast<DX12CommandQueue&>(queues.Get(CommandType::Graphics)).GetTracyCtx(), name);
                case CommandType::Compute:
                    return std::make_shared<DX12ComputeCommandList>(*backend.GetDevice().Get(), backend.GetLogger(), static_cast<DX12BindlessUtils&>(bindings), static_cast<DX12CommandQueue&>(queues.Get(CommandType::Compute)).GetTracyCtx(), name);
                case CommandType::Copy:
                    return std::make_shared<DX12CopyCommandList>(*backend.GetDevice().Get(), backend.GetLogger(), static_cast<DX12CommandQueue&>(queues.Get(CommandType::Copy)).GetTracyCtx(), name);
                default:
                    throw std::runtime_error("Invalid command type.");
            }
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            switch (type) {
                case CommandType::Graphics:
                    return std::make_shared<VulkanGraphicsCommandBuffer>(backend.GetDevice(), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Graphics)).GetCommandPool(), backend.GetLogger(), static_cast<VulkanBindlessUtils&>(bindings), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Graphics)).GetTracyCtx(), name);
                case CommandType::Compute:
                    return std::make_shared<VulkanComputeCommandBuffer>(backend.GetDevice(), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Compute)).GetCommandPool(), backend.GetLogger(), static_cast<VulkanBindlessUtils&>(bindings), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Compute)).GetTracyCtx(), name);
                case CommandType::Copy:
                    return std::make_shared<VulkanTransferCommandBuffer>(backend.GetDevice(), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Copy)).GetCommandPool(), backend.GetLogger(), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Copy)).GetTracyCtx(), name);
                default:
                    throw std::runtime_error("Invalid command type.");
            }
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            switch (type) {
                case CommandType::Graphics:
                    return std::make_shared<MockGraphicsCommandContext>(name);
                case CommandType::Compute:
                    return std::make_shared<MockComputeCommandContext>(name);
                case CommandType::Copy:
                    return std::make_shared<MockCopyCommandContext>(name);
                default:
                    utils::unreachable();
            }
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto SwapChain::Create(Device& device, CommandQueue& queue, SwapChainDesc desc) -> std::shared_ptr<SwapChain> {
    validate_queue(device, queue, CommandType::Graphics);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12SwapChain>(backend.GetFactory(), *static_cast<DX12CommandQueue&>(queue).GetDX12Queue().Get(), queue, backend.GetLogger(), std::move(desc));
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanSwapChain>(backend.GetDevice(), backend.GetCustomAllocator(), backend.GetInstance(), backend.GetPhysicalDevice(), static_cast<VulkanCommandQueue&>(queue).GetVkQueue(), static_cast<VulkanCommandQueue&>(queue).GetFamilyIndex(), backend.GetLogger(), std::move(desc));
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockSwapChain>(&backend, std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto GPUBuffer::Create(Device& device, GPUBufferDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GPUBuffer> {
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12GPUBuffer>(*backend.GetAllocator().Get(), backend.GetLogger(), std::move(desc), initial_data);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanBuffer>(backend.GetDevice(), backend.GetCustomAllocator(), backend.GetVmaAllocator(), backend.GetLogger(), std::move(desc), initial_data);
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockGPUBuffer>(&backend, std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto GPUBufferView::Create(Device& device, BindlessUtils& bindings, GPUBufferViewDesc desc) -> std::shared_ptr<GPUBufferView> {
    validate_bindings(device, bindings);
    if (!desc.buffer) throw std::invalid_argument("buffer is nullptr");
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto&                                backend  = dynamic_cast<DX12Device&>(device);
            const auto*                          resource = dynamic_cast<const DX12GPUBuffer*>(desc.buffer.get());
            Microsoft::WRL::ComPtr<ID3D12Device> owner;
            if (!resource || interop::failed(resource->resource->GetDevice(IID_PPV_ARGS(&owner))) || owner.Get() != backend.GetDevice().Get()) throw std::invalid_argument("buffer belongs to another device");
            return std::make_shared<DX12GPUBufferView>(static_cast<DX12BindlessUtils&>(bindings), GPUBuffer::GetStorageViewRequirements(device), std::move(desc));
        }
#endif
        case Device::Type::Vulkan: {
            auto&       backend  = dynamic_cast<VulkanDevice&>(device);
            const auto* resource = dynamic_cast<const VulkanBuffer*>(desc.buffer.get());
            if (!resource || resource->buffer->getDevice() != *backend.GetDevice()) throw std::invalid_argument("buffer belongs to another device");
            return std::make_shared<VulkanBufferView>(backend.GetDevice(), backend.GetCustomAllocator(), static_cast<VulkanBindlessUtils&>(bindings), GPUBuffer::GetStorageViewRequirements(device), std::move(desc));
        }
        case Device::Type::Mock: {
            auto&       backend  = dynamic_cast<MockDevice&>(device);
            const auto* resource = dynamic_cast<const MockGPUBuffer*>(desc.buffer.get());
            if (!resource || resource->m_Owner != &backend) throw std::invalid_argument("buffer belongs to another device");
            return std::make_shared<MockGPUBufferView>(bindings, GPUBuffer::GetStorageViewRequirements(device), std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto Texture::Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    validate_queue(device, queues.Get(CommandType::Graphics), CommandType::Graphics);
    validate_queue(device, queues.Get(CommandType::Copy), CommandType::Copy);
    validate_bindings(device, bindings);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            auto  texture = std::make_shared<DX12Texture>(*backend.GetAllocator().Get(), backend.GetLogger(), std::move(desc), initial_data);
            initialize_dx12_texture(backend, queues, bindings, *texture, initial_data);
            return texture;
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            auto  texture = std::make_shared<VulkanImage>(backend.GetDevice(), backend.GetCustomAllocator(), backend.GetVmaAllocator(), backend.GetLogger(), std::move(desc), initial_data);
            initialize_vulkan_texture(backend, queues, *texture, initial_data);
            return texture;
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockTexture>(&backend, std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto TextureView::Create(Device& device, BindlessUtils& bindings, TextureViewDesc desc) -> std::shared_ptr<TextureView> {
    validate_bindings(device, bindings);
    if (!desc.texture) throw std::invalid_argument("texture is nullptr");
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto&                                backend  = dynamic_cast<DX12Device&>(device);
            const auto*                          resource = dynamic_cast<const DX12Texture*>(desc.texture.get());
            Microsoft::WRL::ComPtr<ID3D12Device> owner;
            if (!resource || interop::failed(resource->resource->GetDevice(IID_PPV_ARGS(&owner))) || owner.Get() != backend.GetDevice().Get()) throw std::invalid_argument("texture belongs to another device");
            return std::make_shared<DX12TextureView>(*backend.GetDevice().Get(), static_cast<DX12BindlessUtils&>(bindings), backend.GetRTVDescriptorAllocator(), backend.GetDSVDescriptorAllocator(), backend.GetLogger(), std::move(desc));
        }
#endif
        case Device::Type::Vulkan: {
            auto&       backend  = dynamic_cast<VulkanDevice&>(device);
            const auto* resource = dynamic_cast<const VulkanImage*>(desc.texture.get());
            if (!resource || resource->native_device != *backend.GetDevice()) throw std::invalid_argument("texture belongs to another device");
            return std::make_shared<VulkanTextureView>(backend.GetDevice(), backend.GetCustomAllocator(), static_cast<VulkanBindlessUtils&>(bindings), backend.GetLogger(), std::move(desc));
        }
        case Device::Type::Mock: {
            auto&       backend  = dynamic_cast<MockDevice&>(device);
            const auto* resource = dynamic_cast<const MockTexture*>(desc.texture.get());
            if (!resource || resource->m_Owner != &backend) throw std::invalid_argument("texture belongs to another device");
            return std::make_shared<MockTextureView>(bindings, std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto Sampler::Create(Device& device, BindlessUtils& bindings, SamplerDesc desc) -> std::shared_ptr<Sampler> {
    validate_bindings(device, bindings);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12Sampler>(static_cast<DX12BindlessUtils&>(bindings), backend.GetLogger(), std::move(desc));
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanSampler>(backend.GetDevice(), backend.GetCustomAllocator(), static_cast<VulkanBindlessUtils&>(bindings), std::move(desc));
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockSampler>(bindings, std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto Shader::Create(Device& device, const ShaderCompiler& compiler, ShaderDesc desc) -> std::shared_ptr<Shader> {
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12Shader>(compiler, backend.GetLogger(), std::move(desc));
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanShader>(backend.GetDevice(), backend.GetCustomAllocator(), compiler, std::move(desc));
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockShader>(std::move(desc));
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto RenderPipeline::Create(Device& device, BindlessUtils& bindings, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) -> std::shared_ptr<RenderPipeline> {
    validate_bindings(device, bindings);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12RenderPipeline>(*backend.GetDevice().Get(), *static_cast<DX12BindlessUtils&>(bindings).GetBindlessRootSignature().Get(), backend.GetLogger(), std::move(desc), shaders);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanRenderPipeline>(backend.GetDevice(), backend.GetCustomAllocator(), static_cast<VulkanBindlessUtils&>(bindings).mapping_info, backend.GetLogger(), std::move(desc), shaders);
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockRenderPipeline>(std::move(desc), shaders);
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto ComputePipeline::Create(Device& device, BindlessUtils& bindings, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) -> std::shared_ptr<ComputePipeline> {
    validate_bindings(device, bindings);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12ComputePipeline>(*backend.GetDevice().Get(), *static_cast<DX12BindlessUtils&>(bindings).GetBindlessRootSignature().Get(), backend.GetLogger(), std::move(desc), cs);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanComputePipeline>(backend.GetDevice(), backend.GetCustomAllocator(), static_cast<VulkanBindlessUtils&>(bindings).mapping_info, backend.GetLogger(), std::move(desc), cs);
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockComputePipeline>(std::move(desc), cs);
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto GraphicsCommandContext::Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return std::static_pointer_cast<GraphicsCommandContext>(CommandContext::Create(device, queues, bindings, CommandType::Graphics, name));
}

auto ComputeCommandContext::Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return std::static_pointer_cast<ComputeCommandContext>(CommandContext::Create(device, queues, bindings, CommandType::Compute, name));
}

auto CopyCommandContext::Create(Device& device, CommandQueues& queues, std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    validate_queue(device, queues.Get(CommandType::Copy), CommandType::Copy);
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12CopyCommandList>(*backend.GetDevice().Get(), backend.GetLogger(), static_cast<DX12CommandQueue&>(queues.Get(CommandType::Copy)).GetTracyCtx(), name);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanTransferCommandBuffer>(backend.GetDevice(), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Copy)).GetCommandPool(), backend.GetLogger(), static_cast<VulkanCommandQueue&>(queues.Get(CommandType::Copy)).GetTracyCtx(), name);
        }
        case Device::Type::Mock: {
            auto& backend = dynamic_cast<MockDevice&>(device);
            return std::make_shared<MockCopyCommandContext>(name);
        }
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto CommandQueue::Create(Device& device, CommandType type, std::string_view name) -> std::shared_ptr<CommandQueue> {
    if (!magic_enum::enum_contains(type)) throw std::invalid_argument("Invalid command type");
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_shared<DX12CommandQueue>(*backend.GetDevice().Get(), backend.GetLogger(), type, name);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_shared<VulkanCommandQueue>(backend.GetDevice(), backend.GetPhysicalDevice(), backend.GetCustomAllocator(), backend.GetLogger(), type, name, backend.GetQueueFamilyIndex());
        }
        case Device::Type::Mock:
            return std::make_shared<MockCommandQueue>(&device, type, name);
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

auto BindlessUtils::Create(Device& device, std::string_view name) -> std::unique_ptr<BindlessUtils> {
    switch (device.device_type) {
#ifdef _WIN32
        case Device::Type::DX12: {
            auto& backend = dynamic_cast<DX12Device&>(device);
            return std::make_unique<DX12BindlessUtils>(*backend.GetDevice().Get(), backend.GetLogger(), name);
        }
#endif
        case Device::Type::Vulkan: {
            auto& backend = dynamic_cast<VulkanDevice&>(device);
            return std::make_unique<VulkanBindlessUtils>(backend.GetDevice(), backend.GetPhysicalDevice(), backend.GetVmaAllocator(), name);
        }
        case Device::Type::Mock:
            return std::make_unique<MockBindlessUtils>(&device, name);
        default:
            throw std::invalid_argument("Unsupported graphics backend");
    }
}

}  // namespace hitagi::gfx
