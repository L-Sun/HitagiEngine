module;
#include <utility>

#include "interop/tracy_macros.hpp"

export module gfx.vulkan:resource;
#ifdef _WIN32
import interop.win32;
#endif
import interop.sdl;
import interop.fmt;
import interop.magic_enum;
import interop.spdlog;
import interop.vma;
import interop.vulkan;
import interop.tracy;
import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :command_queue;
import :bindless;
import :utils;
import :configs;

export namespace hitagi::gfx {

struct VulkanBuffer final : public GPUBuffer {
    VulkanBuffer(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VmaAllocator allocator, std::shared_ptr<spdlog::logger> logger, GPUBufferDesc desc, std::span<const std::byte> initial_data);
    ~VulkanBuffer() final;

    auto GetAllocationSize() const noexcept -> std::uint64_t final { return m_AllocationSize; }
    auto Map() -> std::byte* final;
    void UnMap() final;

    std::unique_ptr<vk::raii::Buffer> buffer;
    VmaAllocation                     allocation       = nullptr;
    std::uint64_t                     m_AllocationSize = 0;

    VmaAllocator                    m_Allocator;
    std::shared_ptr<spdlog::logger> m_Logger;
    std::mutex                      map_mutex;
    std::uint16_t                   mapped_count{0};
};

struct VulkanBufferView final : public GPUBufferView {
    VulkanBufferView(VulkanBindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc);
};

struct VulkanImage final : public Texture {
    VulkanImage(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VmaAllocator allocator, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data);
    VulkanImage(const vk::raii::Device& device, const VulkanSwapChain& swap_chian, std::uint32_t index);
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage(VulkanImage&&)      = default;
    ~VulkanImage() final;

    auto GetAllocationSize() const noexcept -> std::uint64_t final { return m_AllocationSize; }

    std::optional<vk::raii::Image> image;
    vk::Image                      image_handle;
    const VulkanSwapChain*         swap_chain = nullptr;

    VmaAllocator  m_Allocator = nullptr;
    vk::Device    native_device;
    VmaAllocation allocation       = nullptr;
    std::uint64_t m_AllocationSize = 0;
};

struct VulkanTextureView final : public TextureView {
    VulkanTextureView(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc);

    std::optional<vk::raii::ImageView> image_view;
};

struct VulkanSampler final : public Sampler {
    VulkanSampler(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, SamplerDesc desc);

    std::unique_ptr<vk::raii::Sampler> sampler;
};

class VulkanSwapChain final : public SwapChain {
public:
    struct SemaphorePair {
        SemaphorePair() = default;
        SemaphorePair(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, std::string_view name);
        std::shared_ptr<vk::raii::Semaphore> image_available;
        std::shared_ptr<vk::raii::Semaphore> presentable;
    };

    VulkanSwapChain(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physical_device, std::shared_ptr<VulkanNativeQueue> queue, std::span<const std::shared_ptr<VulkanNativeQueue>> device_queues, std::uint32_t queue_family_index, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc);

    auto AcquireTextureForRendering() -> utils::optional_ref<Texture> final;

    inline auto GetWidth() const noexcept -> std::uint32_t final { return m_Size.x; };
    inline auto GetHeight() const noexcept -> std::uint32_t final { return m_Size.y; };
    inline auto GetFormat() const noexcept -> Format final { return m_Format; }

    void Present() final;
    void Resize() final;

    inline auto& GetVkSwapChain() const noexcept { return *m_SwapChain; }

    inline const auto& GetSemaphores() const noexcept { return m_CurrentSemaphores; }

private:
    const vk::raii::Device&                             m_Device;
    const vk::AllocationCallbacks&                      m_Callbacks;
    const vk::raii::PhysicalDevice&                     m_PhysicalDevice;
    std::shared_ptr<VulkanNativeQueue>                  m_Queue;
    std::span<const std::shared_ptr<VulkanNativeQueue>> m_DeviceQueues;
    std::uint32_t                                       m_QueueFamilyIndex;
    std::shared_ptr<spdlog::logger>                     m_Logger;

    void CreateSwapChain();
    void CreateImageViews();

    std::unique_ptr<vk::raii::SurfaceKHR>   m_Surface;
    std::unique_ptr<vk::raii::SwapchainKHR> m_SwapChain;
    struct RetiredSwapChain {
        std::unique_ptr<vk::raii::SwapchainKHR> swap_chain;
        std::pmr::vector<SemaphorePair>         semaphores;
    };
    std::pmr::vector<RetiredSwapChain>         m_RetiredSwapChains;
    int                                        m_RetirementImage = -1;
    math::vec2u                                m_Size;
    Format                                     m_Format;
    std::uint32_t                              m_NumImages;
    std::pmr::vector<std::unique_ptr<Texture>> m_Images;

    int                             m_CurrentIndex       = -1;
    std::uint32_t                   m_NextSemaphoreIndex = 0;
    SemaphorePair                   m_CurrentSemaphores;
    std::pmr::vector<SemaphorePair> m_SemaphorePairs;
};

struct VulkanShader final : public Shader {
    VulkanShader(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, ShaderDesc desc, CompiledShader compiled);

    auto GetSPIRVData() const noexcept -> std::span<const std::byte> final;

    vk::raii::ShaderModule shader;
};

struct VulkanRenderPipeline final : public RenderPipeline {
    VulkanRenderPipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

struct VulkanComputePipeline final : public ComputePipeline {
    VulkanComputePipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

class VulkanDescriptorHeap final {
public:
    VulkanDescriptorHeap(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, VmaAllocator allocator);
    VulkanDescriptorHeap(const VulkanDescriptorHeap&)            = delete;
    VulkanDescriptorHeap& operator=(const VulkanDescriptorHeap&) = delete;

    void Write(std::uint32_t index, const GPUBufferViewDesc& desc, std::uint64_t size);
    void Write(std::uint32_t index, const TextureView& view);
    void Write(std::uint32_t index, const Sampler& sampler);
    void Bind(const vk::raii::CommandBuffer& command_buffer, vk::PipelineBindPoint bind_point) const;
    void PushMetaInfo(const vk::raii::CommandBuffer& command_buffer, const BindlessMetaInfo& info) const;
    void ConfigureShader(vk::PipelineShaderStageCreateInfo& info) const { info.pNext = &m_MappingInfo; }

    template <typename PipelineCreateInfo>
    auto CreatePipeline(PipelineCreateInfo info, const vk::AllocationCallbacks& callbacks) const -> std::unique_ptr<vk::raii::Pipeline> {
        const vk::PipelineCreateFlags2CreateInfo flags{
            .pNext = info.pNext,
            .flags = vk::PipelineCreateFlagBits2::eDescriptorHeapEXT,
        };
        info.pNext  = &flags;
        info.layout = nullptr;
        return std::make_unique<vk::raii::Pipeline>(m_Device, nullptr, info, callbacks);
    }

private:
    const vk::raii::Device& m_Device;
    struct Heap {
        ~Heap();
        VmaAllocator        allocator  = nullptr;
        VmaAllocation       allocation = nullptr;
        VkBuffer            buffer     = interop::null_handle;
        std::byte*          mapped     = nullptr;
        vk::BindHeapInfoEXT bind_info;
    };
    std::array<Heap, 2>                                  m_Heaps;
    std::array<vk::DescriptorSetAndBindingMappingEXT, 4> m_Mappings;
    std::array<std::uint32_t, 4>                         m_DescriptorSizes;
    std::array<std::uint32_t, 4>                         m_Offsets;

    vk::ShaderDescriptorSetAndBindingMappingInfoEXT m_MappingInfo;
};

class VulkanDescriptorIndexing final {
public:
    VulkanDescriptorIndexing(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device);
    void Write(std::uint32_t index, const GPUBufferViewDesc& desc, std::uint64_t size);
    void Write(std::uint32_t index, const TextureView& view);
    void Write(std::uint32_t index, const Sampler& sampler);
    void Bind(const vk::raii::CommandBuffer& command_buffer, vk::PipelineBindPoint bind_point) const;
    void PushMetaInfo(const vk::raii::CommandBuffer& command_buffer, const BindlessMetaInfo& info) const;
    void ConfigureShader(vk::PipelineShaderStageCreateInfo&) const {}

    template <typename PipelineCreateInfo>
    auto CreatePipeline(PipelineCreateInfo info, const vk::AllocationCallbacks& callbacks) const -> std::unique_ptr<vk::raii::Pipeline> {
        info.layout = **m_PipelineLayout;
        return std::make_unique<vk::raii::Pipeline>(m_Device, nullptr, info, callbacks);
    }

private:
    const vk::raii::Device&                        m_Device;
    std::uint64_t                                  m_MaxStorageBufferRange = 0;
    std::unique_ptr<vk::raii::DescriptorSetLayout> m_SetLayout;
    std::unique_ptr<vk::raii::PipelineLayout>      m_PipelineLayout;
    std::unique_ptr<vk::raii::DescriptorPool>      m_DescriptorPool;
    std::unique_ptr<vk::raii::DescriptorSet>       m_DescriptorSet;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanBuffer::VulkanBuffer(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VmaAllocator allocator, std::shared_ptr<spdlog::logger> logger, GPUBufferDesc desc, std::span<const std::byte> initial_data)
    : GPUBuffer(std::move(desc)), m_Allocator(allocator), m_Logger(std::move(logger)) {
    logger = m_Logger;

    if (Size() == 0) {
        const auto error_message = fmt::format(
            "GPU buffer({}) size must be larger than 0",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    logger->trace("Create buffer({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        const vk::BufferCreateInfo buffer_create_info = {
            .size  = Size(),
            .usage = to_vk_buffer_usage(m_Desc.usages),
        };
        buffer = std::make_unique<vk::raii::Buffer>(device, buffer_create_info, callbacks);
    }

    logger->trace("Allocate buffer({}) memory", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        VmaAllocationCreateInfo allocation_create_info{};
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead) ||
            utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

            if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Vertex) ||
                utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Index) ||
                utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::StorageRead)) {
                allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
        } else if (!initial_data.empty() && utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::CopyDst)) {
            // ReBAR: DEVICE_LOCAL + HOST_VISIBLE for direct VRAM write
            allocation_create_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        } else {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead)) {
            allocation_create_info.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        VmaAllocationInfo allocation_info;
        if (VK_SUCCESS != vmaAllocateMemoryForBuffer(
                              allocator,
                              **buffer,
                              &allocation_create_info,
                              &allocation,
                              &allocation_info)) {
            auto error_message = fmt::format("failed to allocate memory for buffer({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        m_AllocationSize = allocation_info.size;
        vmaBindBufferMemory(allocator, allocation, **buffer);
    }

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to buffer({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));

        if (initial_data.size() > Size()) {
            logger->warn(
                "the initial data size({}) is larger than gpu buffer({}) size({}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                fmt::styled(GetName(), fmt::fg(fmt::color::green)),
                fmt::styled(Size(), fmt::fg(fmt::color::green)));
        }

        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            auto mapped_ptr = Map();
            std::memcpy(mapped_ptr, initial_data.data(), std::min<std::size_t>(initial_data.size(), Size()));
            UnMap();
        } else if (utils::has_flag(desc.usages, GPUBufferUsageFlags::CopyDst)) {
            // Direct VRAM write via ReBAR (DEVICE_LOCAL + HOST_VISIBLE)
            void* mapped_ptr = nullptr;
            if (VK_SUCCESS != vmaMapMemory(allocator, allocation, &mapped_ptr)) {
                auto error_message = fmt::format("failed to map ReBAR buffer({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
                logger->error(error_message);
                throw std::runtime_error(error_message);
            }
            std::memcpy(mapped_ptr, initial_data.data(), std::min<std::size_t>(initial_data.size(), Size()));
            vmaUnmapMemory(allocator, allocation);
        } else {
            auto error_message = fmt::format(
                "Can not initialize gpu buffer({}) using upload heap without the flag {} or {}, the actual flags are {}",
                fmt::styled(GetName(), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(GPUBufferUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(GPUBufferUsageFlags::MapWrite), fmt::fg(fmt::color::green)),
                fmt::styled(format_as(m_Desc.usages), fmt::fg(fmt::color::red)));

            logger->error(error_message);
            vmaFreeMemory(m_Allocator, allocation);
            throw std::invalid_argument(error_message);
        }
    }

    create_vk_debug_object_info(*buffer, GetName(), device);
}

VulkanBufferView::VulkanBufferView(VulkanBindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc)
    : GPUBufferView(bindings, storage_requirements, std::move(desc)) {
    if (RequiresBindlessHandle()) m_BindlessHandle = bindings.CreateBindlessHandle(m_Desc, Size());
}

VulkanBuffer::~VulkanBuffer() {
    if (allocation) vmaFreeMemory(m_Allocator, allocation);
}

auto VulkanBuffer::Map() -> std::byte* {
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
    if (VK_SUCCESS != vmaMapMemory(m_Allocator,
                                   allocation,
                                   reinterpret_cast<void**>(&mapped_ptr))) {
        const auto error_message = fmt::format(
            "failed to map GPU buffer({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead)) {
        if (const auto result = vmaInvalidateAllocation(m_Allocator, allocation, 0, interop::whole_size);
            result != VK_SUCCESS) {
            const auto error_message = fmt::format(
                "failed to invalidate GPU buffer({}) for host read",
                fmt::styled(GetName(), fmt::fg(fmt::color::green)));
            logger->error(error_message);
            vmaUnmapMemory(m_Allocator, allocation);
            throw std::runtime_error(error_message);
        }
    }

    std::scoped_lock lock(map_mutex);
    mapped_count++;
    return mapped_ptr;
}

void VulkanBuffer::UnMap() {
    std::scoped_lock lock(map_mutex);

    if (mapped_count == 0) {
        const auto error_message = fmt::format(
            "Can not unmap GPU buffer({}) without map it!",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    mapped_count--;

    if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
        if (const auto result = vmaFlushAllocation(m_Allocator, allocation, 0, interop::whole_size);
            result != VK_SUCCESS) {
            const auto error_message = fmt::format(
                "failed to flush GPU buffer({}) for host write",
                fmt::styled(GetName(), fmt::fg(fmt::color::green)));
            m_Logger->error(error_message);
            throw std::runtime_error(error_message);
        }
    }

    vmaUnmapMemory(m_Allocator, allocation);
}

VulkanImage::VulkanImage(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VmaAllocator allocator, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data)
    : Texture(std::move(desc)), m_Allocator(allocator), native_device(*device) {
    logger->trace("Create Texture({})...", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        if (m_Desc.width == 0 || m_Desc.height == 0 || m_Desc.depth == 0) {
            const auto error_message = fmt::format(
                "the texture({}) size({} x {} x {}) can not be zero!",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.width, fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.height, fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.depth, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }

        image        = vk::raii::Image(device, to_vk_image_create_info(m_Desc), callbacks);
        image_handle = **image;
        create_vk_debug_object_info(image.value(), GetName(), device);
    }

    logger->trace("Allocate memory for Texture({})...", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        const VmaAllocationCreateInfo vma_alloc_create_info{
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VmaAllocationInfo allocation_info;
        if (VK_SUCCESS != vmaAllocateMemoryForImage(
                              allocator,
                              *image.value(),
                              &vma_alloc_create_info,
                              &allocation,
                              &allocation_info)) {
            auto error_message = fmt::format("failed to allocate memory for texture({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        m_AllocationSize = allocation_info.size;
        vmaBindImageMemory(allocator, allocation, **image);
    }
}

VulkanImage::VulkanImage(const vk::raii::Device& device, const VulkanSwapChain& _swap_chian, std::uint32_t index)
    : Texture(
          {
              .name        = std::pmr::string(std::format("{}-texture-{}", _swap_chian.GetName(), index)),
              .width       = _swap_chian.GetWidth(),
              .height      = _swap_chian.GetHeight(),
              .format      = _swap_chian.GetFormat(),
              .clear_value = _swap_chian.GetDesc().clear_color,
              .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopyDst,
          }),
      swap_chain(&_swap_chian),
      native_device(*device) {
    const auto _images = _swap_chian.GetVkSwapChain().getImages();
    create_vk_debug_object_info(_images.at(index), GetName(), device);

    image_handle = _images.at(index);
}

VulkanTextureView::VulkanTextureView(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc)
    : TextureView(bindings, std::move(desc)) {
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

    auto& vulkan_image = dynamic_cast<VulkanImage&>(*m_Desc.texture);
    image_view         = vk::raii::ImageView(device, to_vk_image_view_create_info(m_Desc, vulkan_image.image_handle), callbacks);
    create_vk_debug_object_info(image_view.value(), GetName(), device);

    if (RequiresBindlessHandle()) m_BindlessHandle = bindings.CreateBindlessHandle(*this);
}

VulkanImage::~VulkanImage() {
    if (allocation) vmaFreeMemory(m_Allocator, allocation);
}

VulkanSampler::VulkanSampler(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, SamplerDesc desc) : Sampler(bindings, std::move(desc)) {
    sampler = std::make_unique<vk::raii::Sampler>(device, to_vk_sampler_create_info(m_Desc), callbacks);
    create_vk_debug_object_info(*sampler, GetName(), device);
    if (RequiresBindlessHandle()) m_BindlessHandle = bindings.CreateBindlessHandle(*this);
}

VulkanSwapChain::SemaphorePair::SemaphorePair(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, std::string_view name)
    : image_available(std::make_shared<vk::raii::Semaphore>(device, vk::SemaphoreCreateInfo{}, callbacks)),
      presentable(std::make_shared<vk::raii::Semaphore>(device, vk::SemaphoreCreateInfo{}, callbacks)) {
    create_vk_debug_object_info(*image_available, std::format("{}-image-available", name), device);
    create_vk_debug_object_info(*presentable, std::format("{}-presentable", name), device);
}

VulkanSwapChain::VulkanSwapChain(
    const vk::raii::Device&                             device,
    const vk::AllocationCallbacks&                      callbacks,
    const vk::raii::Instance&                           instance,
    const vk::raii::PhysicalDevice&                     physical_device,
    std::shared_ptr<VulkanNativeQueue>                  queue,
    std::span<const std::shared_ptr<VulkanNativeQueue>> device_queues,
    std::uint32_t                                       queue_family_index,
    std::shared_ptr<spdlog::logger>                     logger,
    SwapChainDesc                                       desc)
    : SwapChain(std::move(desc)),
      m_Device(device),
      m_Callbacks(callbacks),
      m_PhysicalDevice(physical_device),
      m_Queue(std::move(queue)),
      m_DeviceQueues(device_queues),
      m_QueueFamilyIndex(queue_family_index),
      m_Logger(std::move(logger)) {
    switch (m_Desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            auto                          h_wnd = static_cast<HWND>(m_Desc.window.ptr);
            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandleW(nullptr),
                .hwnd      = h_wnd,
            };
            m_Surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface_create_info, callbacks);
        } break;
#endif
        case utils::Window::Type::SDL3: {
            auto                  sdl_window = static_cast<SDL_Window*>(m_Desc.window.ptr);
            vk::SurfaceKHR::CType surface{};
            if (!SDL_Vulkan_CreateSurface(sdl_window, static_cast<vk::Instance::CType>(*instance), callbacks, &surface)) {
                const auto error_message = std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
                m_Logger->error(error_message);
                throw std::runtime_error(error_message);
            }
            m_Surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface, callbacks);
        } break;
        default: {
            const auto error_message = std::format("Vulkan swap chain does not support window type {}", magic_enum::enum_name(m_Desc.window.type));
            m_Logger->error(error_message);
            throw std::runtime_error(error_message);
        }
    }

    CreateSwapChain();
    CreateImageViews();
}

auto VulkanSwapChain::AcquireTextureForRendering() -> utils::optional_ref<Texture> {
    ZoneScopedN("Vulkan Acquire Swapchain");
    if (m_SwapChain == nullptr) return {};

    if (m_CurrentIndex != -1) {
        return *m_Images[m_CurrentIndex];
    }

    if (m_SemaphorePairs.empty()) {
        return {};
    }

    auto& acquire_semaphores = m_SemaphorePairs[m_NextSemaphoreIndex];
    m_NextSemaphoreIndex     = (m_NextSemaphoreIndex + 1) % static_cast<std::uint32_t>(m_SemaphorePairs.size());

    // Reacquiring the replacement's first presented image proves that the
    // presentation engine has finished with all preceding swapchains too.
    std::optional<vk::raii::Fence> retirement_fence;
    if (m_RetirementImage != -1) {
        retirement_fence.emplace(m_Device, vk::FenceCreateInfo{}, m_Callbacks);
    }
    auto [result, index] = m_SwapChain->acquireNextImage(
        std::numeric_limits<std::uint64_t>::max(),
        **acquire_semaphores.image_available,
        retirement_fence ? **retirement_fence : vk::Fence{});

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire next image");
    }

    if (retirement_fence) {
        const auto waited = m_Device.waitForFences(**retirement_fence, true, std::numeric_limits<std::uint64_t>::max());
        if (waited != vk::Result::eSuccess) throw std::runtime_error("failed to wait for swapchain retirement");
        if (index == static_cast<std::uint32_t>(m_RetirementImage)) {
            m_RetiredSwapChains.clear();
            m_RetirementImage = -1;
        }
    }

    m_CurrentIndex                      = static_cast<int>(index);
    m_CurrentSemaphores.image_available = acquire_semaphores.image_available;
    m_CurrentSemaphores.presentable     = m_SemaphorePairs[index].presentable;

    return *m_Images[m_CurrentIndex];
}

void VulkanSwapChain::Present() {
    ZoneScopedN("Vulkan Present");
    if (!m_SwapChain || m_CurrentIndex == -1) return;

    const auto& queue = m_Queue;

    auto index                 = static_cast<std::uint32_t>(m_CurrentIndex);
    auto presentable_semaphore = m_CurrentSemaphores.presentable;
    m_CurrentIndex             = -1;
    m_CurrentSemaphores        = {};

    std::scoped_lock lock{queue->mutex};
    auto             result = queue->queue.presentKHR(vk::PresentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &(**presentable_semaphore),
        .swapchainCount     = 1,
        .pSwapchains        = &(**m_SwapChain),
        .pImageIndices      = &index,
    });

    if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
        if (!m_RetiredSwapChains.empty() && m_RetirementImage == -1) {
            m_RetirementImage = static_cast<int>(index);
        }
    } else {
        m_Logger->error("failed to present swap chain image");
    }
}

void VulkanSwapChain::Resize() {
    wait_vulkan_device_idle(m_Device, m_DeviceQueues);
    m_CurrentIndex       = -1;
    m_NextSemaphoreIndex = 0;
    m_CurrentSemaphores  = {};
    CreateSwapChain();
    CreateImageViews();
}

void VulkanSwapChain::CreateSwapChain() {
    math::vec2u window_size;
    switch (m_Desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            if (!IsWindow(static_cast<HWND>(m_Desc.window.ptr))) {
                m_Images.clear();
                m_SemaphorePairs.clear();
                m_SwapChain.reset();
                m_RetiredSwapChains.clear();
                m_RetirementImage = -1;
                return;
            }
            RECT rect;
            GetClientRect(static_cast<HWND>(m_Desc.window.ptr), &rect);
            window_size.x = rect.right - rect.left;
            window_size.y = rect.bottom - rect.top;
        } break;
#endif
        case utils::Window::Type::SDL3:
            window_size = get_sdl3_window_size(static_cast<SDL_Window*>(m_Desc.window.ptr));
            break;
    }

    if (window_size.x == 0 || window_size.y == 0) {
        return;
    }

    // we use graphics queue as present queue
    const auto& physical_device = m_PhysicalDevice;
    if (!physical_device.getSurfaceSupportKHR(m_QueueFamilyIndex, **m_Surface)) {
        throw std::runtime_error(std::format(
            "The graphics queue({}) of physical device({}) can not support surface(window.ptr: {})",
            m_QueueFamilyIndex,
            physical_device.getProperties().deviceName.data(),
            m_Desc.window.ptr));
    }

    const auto surface_capabilities    = physical_device.getSurfaceCapabilitiesKHR(**m_Surface);
    const auto supported_formats       = physical_device.getSurfaceFormatsKHR(**m_Surface);
    const auto supported_present_modes = physical_device.getSurfacePresentModesKHR(**m_Surface);

    const auto surface_format = choose_surface_format(supported_formats);
    m_Format                  = from_vk_format(surface_format.format);

    if (std::find_if(supported_present_modes.begin(), supported_present_modes.end(), [](const auto& present_mode) {
            return present_mode == vk::PresentModeKHR::eFifo;
        }) == supported_present_modes.end()) {
        throw std::runtime_error(std::format(
            "The physical device({}) can not support surface(window.ptr: {}) with present mode: VK_PRESENT_MODE_FIFO_KHR",
            physical_device.getProperties().deviceName.data(),
            m_Desc.window.ptr));
    }

    if (surface_capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max()) {
        // surface size is undefined, so we define it
        m_Size.x = std::clamp(window_size.x, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        m_Size.y = std::clamp(window_size.y, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    } else {
        // surface size is defined, so we use it
        m_Size.x = surface_capabilities.currentExtent.width;
        m_Size.y = surface_capabilities.currentExtent.height;
    }
    m_NumImages = surface_capabilities.minImageCount;

    vk::SurfaceTransformFlagBitsKHR preTransform =
        (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
            ? vk::SurfaceTransformFlagBitsKHR::eIdentity
            : surface_capabilities.currentTransform;

    vk::CompositeAlphaFlagBitsKHR composite_alpha =
        (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)           ? vk::CompositeAlphaFlagBitsKHR::eOpaque
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)  ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)        ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                                                                          : vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface          = **m_Surface,
        .minImageCount    = m_NumImages,
        .imageFormat      = to_vk_format(m_Format),
        .imageColorSpace  = surface_format.colorSpace,
        .imageExtent      = vk::Extent2D{.width = m_Size.x, .height = m_Size.y},
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        .preTransform     = preTransform,
        .compositeAlpha   = composite_alpha,
        .presentMode      = vk::PresentModeKHR::eFifo,
        .clipped          = true,
        .oldSwapchain     = m_SwapChain ? **m_SwapChain : vk::SwapchainKHR{},
    };

    // We use graphics queue as present queue, so we do not care the ownership of images
    auto old_swap_chain = std::move(m_SwapChain);
    m_SwapChain         = std::make_unique<vk::raii::SwapchainKHR>(m_Device, swapchain_create_info, m_Callbacks);
    if (old_swap_chain) {
        // Keep the displayed image and its present semaphores alive through
        // the replacement's first completed presentation, including rapid resizes.
        m_RetiredSwapChains.emplace_back(std::move(old_swap_chain), std::move(m_SemaphorePairs));
    }
    m_RetirementImage = -1;
    m_NumImages       = static_cast<std::uint32_t>(m_SwapChain->getImages().size());

    create_vk_debug_object_info(*m_SwapChain, GetName(), m_Device);
}

void VulkanSwapChain::CreateImageViews() {
    m_Images.clear();
    m_SemaphorePairs.clear();
    if (!m_SwapChain) return;
    for (std::uint32_t index = 0; index < m_NumImages; index++) {
        m_SemaphorePairs.emplace_back(m_Device, m_Callbacks, std::format("{}-frame-{}", GetName(), index));
        m_Images.emplace_back(std::make_unique<VulkanImage>(m_Device, *this, index));
    }
}

VulkanShader::VulkanShader(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, ShaderDesc desc, CompiledShader compiled)
    : Shader(std::move(desc), std::move(compiled)),
      shader(
          device,
          {
              .codeSize = m_Compiled.bytecode.GetDataSize(),
              .pCode    = reinterpret_cast<const std::uint32_t*>(m_Compiled.bytecode.GetData()),
          },
          callbacks)

{
    create_vk_debug_object_info(shader, GetName(), device);
}

auto VulkanShader::GetSPIRVData() const noexcept -> std::span<const std::byte> {
    return m_Compiled.bytecode.Span<const std::byte>();
}

VulkanRenderPipeline::VulkanRenderPipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) : RenderPipeline(std::move(desc)) {
    bool has_vertex_shader = false, has_fragment_shader = false;
    for (const auto& shader : shaders) {
        if (shader->GetDesc().type == ShaderType::Vertex) {
            has_vertex_shader = true;
        } else if (shader->GetDesc().type == ShaderType::Pixel) {
            has_fragment_shader = true;
        }
        if (has_vertex_shader && has_fragment_shader) {
            break;
        }
    }

    if (!has_vertex_shader) {
        const auto error_message = fmt::format(
            "Vertex shader is not specified for pipeline({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }
    if (!has_fragment_shader) {
        const auto error_message = fmt::format(
            "Fragment shader is not specified for pipeline({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    // verify all shaders are different type
    if (auto iter = std::adjacent_find(
            shaders.begin(), shaders.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs->GetDesc().type == rhs->GetDesc().type;
            });
        iter != shaders.end()) {
        const auto error_message = fmt::format(
            "shader({}) and shader({}) are same type for pipeline({})",
            fmt::styled((*iter)->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled((*std::next(iter))->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    // verify all shader are vulkan shader
    if (auto iter = std::find_if(
            shaders.begin(), shaders.end(),
            [](const auto& shader) {
                return shader && !std::dynamic_pointer_cast<VulkanShader>(shader);
            });
        iter != shaders.end()) {
        const auto error_message = fmt::format(
            "shader({}) is not vulkan shader for pipeline({})",
            fmt::styled((*iter)->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    std::pmr::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos;
    std::transform(
        shaders.begin(), shaders.end(), std::back_inserter(shader_stage_create_infos),
        [&bindings](const auto& shader) {
            vk::PipelineShaderStageCreateInfo info{
                .stage  = to_vk_shader_stage(shader->GetDesc().type),
                .module = *std::static_pointer_cast<VulkanShader>(shader)->shader,
                .pName  = shader->GetEntryPointName().c_str(),
            };
            bindings.ConfigureShader(info);
            return info;
        });

    const auto assembly_state = to_vk_assembly_state(m_Desc.assembly_state);

    auto vertex_input_attribute_descriptions = m_Desc.vertex_input_layout |
                                               std::ranges::views::enumerate |
                                               std::ranges::views::transform([](const auto& pair) {
                                                   const auto& [location, attribute] = pair;
                                                   return vk::VertexInputAttributeDescription{
                                                       .location = attribute.location == std::numeric_limits<std::uint32_t>::max() ? static_cast<std::uint32_t>(location) : attribute.location,
                                                       .binding  = attribute.binding,
                                                       .format   = to_vk_format(attribute.format),
                                                       .offset   = static_cast<std::uint32_t>(attribute.offset),
                                                   };
                                               }) |
                                               std::ranges::to<std::pmr::vector<vk::VertexInputAttributeDescription>>();

    // The sort all semantic strings according to declaration and assign Location numbers sequentially to the corresponding SPIR-V variables.
    // https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#implicit-location-number-assignment
    auto vertex_input_binding_descriptions = m_Desc.vertex_input_layout |
                                             std::ranges::views::transform([](const auto& attribute) {
                                                 return vk::VertexInputBindingDescription{
                                                     .binding   = attribute.binding,
                                                     .stride    = static_cast<std::uint32_t>(attribute.stride),
                                                     .inputRate = attribute.per_instance
                                                                      ? vk::VertexInputRate::eInstance
                                                                      : vk::VertexInputRate::eVertex,
                                                 };
                                             }) |
                                             std::ranges::to<std::pmr::vector<vk::VertexInputBindingDescription>>();

    auto unique_pred = [](const auto& lhs, const auto& rhs) {
        if (lhs.binding == rhs.binding && (lhs.stride != rhs.stride || lhs.inputRate != rhs.inputRate))
            throw std::invalid_argument("Vertex input binding description is not same");
        return lhs.binding == rhs.binding;
    };

    auto [erase_begin, erase_end] = std::ranges::unique(vertex_input_binding_descriptions, unique_pred);
    vertex_input_binding_descriptions.erase(erase_begin, erase_end);

    const vk::PipelineVertexInputStateCreateInfo vertex_state = {
        .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(vertex_input_binding_descriptions.size()),
        .pVertexBindingDescriptions      = vertex_input_binding_descriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertex_input_attribute_descriptions.size()),
        .pVertexAttributeDescriptions    = vertex_input_attribute_descriptions.data(),
    };
    const auto viewport_state      = vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};
    const auto rasterization_state = to_vk_rasterization_state(m_Desc.rasterization_state);
    const auto depth_stencil_state = to_vk_depth_stencil_state(m_Desc.depth_stencil_state);
    const auto attachment_state    = to_vk_blend_attachment_state(m_Desc.blend_state);

    // TODO : support multisample
    const vk::PipelineMultisampleStateCreateInfo multisample_state{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
    };

    const vk::PipelineColorBlendStateCreateInfo blend_state{
        .logicOpEnable   = m_Desc.blend_state.logic_operation_enable,
        .logicOp         = to_vk_logic_op(m_Desc.blend_state.logic_op),
        .attachmentCount = 1,
        .pAttachments    = &attachment_state,
        .blendConstants  = m_Desc.blend_state.blend_constants.data,
    };

    constexpr std::array dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    const vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{
        .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data(),
    };

    const auto render_format = to_vk_format(desc.render_format);

    const auto depth_format   = to_vk_format(desc.depth_stencil_format);
    const auto stencil_format = (desc.depth_stencil_format == Format::D24_UNORM_S8_UINT ||
                                 desc.depth_stencil_format == Format::D32_FLOAT_S8X24_UINT)
                                    ? depth_format
                                    : vk::Format::eUndefined;

    logger->trace("Create render pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        vk::StructureChain pipeline_create_info = {
            vk::GraphicsPipelineCreateInfo{
                .stageCount          = static_cast<std::uint32_t>(shader_stage_create_infos.size()),
                .pStages             = shader_stage_create_infos.data(),
                .pVertexInputState   = &vertex_state,
                .pInputAssemblyState = &assembly_state,
                .pViewportState      = &viewport_state,
                .pRasterizationState = &rasterization_state,
                .pMultisampleState   = &multisample_state,
                .pDepthStencilState  = &depth_stencil_state,
                .pColorBlendState    = &blend_state,
                .pDynamicState       = &dynamic_state_create_info,
                .renderPass          = nullptr,
            },
            vk::PipelineRenderingCreateInfo{
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &render_format,
                .depthAttachmentFormat   = depth_format,
                .stencilAttachmentFormat = stencil_format,
            },
        };

        pipeline = bindings.CreatePipeline(pipeline_create_info.get(), callbacks);

        switch (pipeline->getConstructorSuccessCode()) {
            case vk::Result::eSuccess:
                break;
            case vk::Result::ePipelineCompileRequired:
                throw std::runtime_error("Pipeline compilation is required");
            default:
                throw std::runtime_error("Failed to create pipeline");
        }
        create_vk_debug_object_info(*pipeline, GetName(), device);
    }
}

VulkanComputePipeline::VulkanComputePipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs)
    : ComputePipeline(std::move(desc)) {
    auto compute_shader = std::dynamic_pointer_cast<VulkanShader>(cs);

    if (compute_shader == nullptr) {
        const auto error_message = fmt::format("Compute shader is not specified for pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    vk::PipelineShaderStageCreateInfo shader_stage_create_info{
        .stage  = vk::ShaderStageFlagBits::eCompute,
        .module = *compute_shader->shader,
        .pName  = compute_shader->GetEntryPointName().c_str(),
    };

    bindings.ConfigureShader(shader_stage_create_info);

    logger->trace("Create compute pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        pipeline = bindings.CreatePipeline(vk::ComputePipelineCreateInfo{.stage = shader_stage_create_info}, callbacks);

        switch (pipeline->getConstructorSuccessCode()) {
            case vk::Result::eSuccess:
                break;
            case vk::Result::ePipelineCompileRequired:
                throw std::runtime_error("Pipeline compilation is required");
            default:
                throw std::runtime_error("Failed to create pipeline");
        }
        create_vk_debug_object_info(*pipeline, GetName(), device);
    }
}

VulkanDescriptorHeap::VulkanDescriptorHeap(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, VmaAllocator allocator)
    : m_Device(device) {
    constexpr auto& counts     = vulkan_bindless_descriptor_counts;
    constexpr auto& bindings   = vulkan_bindless_descriptor_bindings;
    const auto      properties = physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
    const auto&     limits     = properties.get<vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
    if (limits.maxPushDataSize < sizeof(BindlessMetaInfo)) throw std::runtime_error("Insufficient Vulkan push data size");

    m_DescriptorSizes                        = {static_cast<std::uint32_t>(limits.bufferDescriptorSize), static_cast<std::uint32_t>(limits.imageDescriptorSize), static_cast<std::uint32_t>(limits.imageDescriptorSize), static_cast<std::uint32_t>(limits.samplerDescriptorSize)};
    const std::array              alignments = {limits.bufferDescriptorAlignment, limits.imageDescriptorAlignment, limits.imageDescriptorAlignment, limits.samplerDescriptorAlignment};
    std::array<vk::DeviceSize, 2> sizes{};
    // Slang DefaultVkBindlessBindings (BindlessDescriptorOptions.None).
    for (std::uint32_t kind = 0; kind < counts.size(); ++kind) {
        auto& size              = sizes[kind == 3 ? 1 : 0];
        size                    = utils::align(size, alignments[kind]);
        m_Offsets[kind]         = static_cast<std::uint32_t>(size);
        m_DescriptorSizes[kind] = utils::align(m_DescriptorSizes[kind], alignments[kind]);
        size += static_cast<vk::DeviceSize>(m_DescriptorSizes[kind]) * counts[kind];
        m_Mappings[kind] = {
            .descriptorSet = 0,
            .firstBinding  = bindings[kind],
            .bindingCount  = 1,
            .resourceMask  = vk::SpirvResourceTypeFlagBitsEXT::eAll,
            .source        = vk::DescriptorMappingSourceEXT::eHeapWithConstantOffset,
        };
        m_Mappings[kind].sourceData.constantOffset = {.heapOffset = m_Offsets[kind], .heapArrayStride = m_DescriptorSizes[kind]};
    }
    m_MappingInfo = {.mappingCount = static_cast<std::uint32_t>(m_Mappings.size()), .pMappings = m_Mappings.data()};

    const std::array heap_alignments     = {limits.resourceHeapAlignment, limits.samplerHeapAlignment};
    const std::array reserved_alignments = {std::max(limits.bufferDescriptorAlignment, limits.imageDescriptorAlignment), limits.samplerDescriptorAlignment};
    const std::array reserved_sizes      = {limits.minResourceHeapReservedRange, limits.minSamplerHeapReservedRange};
    const std::array max_sizes           = {limits.maxResourceHeapSize, limits.maxSamplerHeapSize};
    for (std::size_t index = 0; index < m_Heaps.size(); ++index) {
        auto& heap                         = m_Heaps[index];
        heap.allocator                     = allocator;
        heap.bind_info.reservedRangeOffset = utils::align(sizes[index], reserved_alignments[index]);
        heap.bind_info.reservedRangeSize   = utils::align(reserved_sizes[index], reserved_alignments[index]);
        const auto size                    = heap.bind_info.reservedRangeOffset + heap.bind_info.reservedRangeSize;
        if (size > max_sizes[index]) throw std::runtime_error("Vulkan descriptor heap capacity exceeds device limits");
        const VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        };
        const VmaAllocationCreateInfo allocation_info{
            .flags         = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage         = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        VmaAllocationInfo allocation{};
        if (vmaCreateBufferWithAlignment(heap.allocator, &buffer_info, &allocation_info, heap_alignments[index], &heap.buffer, &heap.allocation, &allocation) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Vulkan descriptor heap");
        }
        heap.mapped              = static_cast<std::byte*>(allocation.pMappedData);
        heap.bind_info.heapRange = {.address = device.getBufferAddress({.buffer = heap.buffer}), .size = size};
    }
}

VulkanDescriptorHeap::Heap::~Heap() {
    if (buffer) vmaDestroyBuffer(allocator, buffer, allocation);
}

void VulkanDescriptorHeap::Write(std::uint32_t index, const GPUBufferViewDesc& desc, std::uint64_t size) {
    const vk::DeviceAddressRangeEXT range{
        .address = m_Device.getBufferAddress({.buffer = **std::static_pointer_cast<VulkanBuffer>(desc.buffer)->buffer}) + desc.offset,
        .size    = size,
    };
    vk::ResourceDescriptorInfoEXT resource{.type = vk::DescriptorType::eStorageBuffer};
    resource.data.pAddressRange = &range;
    m_Device.writeResourceDescriptorsEXT(
        resource,
        vk::HostAddressRangeEXT{
            .address = m_Heaps[0].mapped + m_Offsets[0] + index * m_DescriptorSizes[0],
            .size    = m_DescriptorSizes[0],
        });
}

void VulkanDescriptorHeap::Write(std::uint32_t index, const TextureView& view) {
    const auto&                      desc      = view.GetDesc();
    const bool                       writable  = desc.type == TextureViewType::ShaderWrite;
    const auto                       kind      = writable ? 2 : 1;
    const auto                       view_info = to_vk_image_view_create_info(desc, std::static_pointer_cast<VulkanImage>(desc.texture)->image_handle);
    const vk::ImageDescriptorInfoEXT image_info{.pView = &view_info, .layout = writable ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::ResourceDescriptorInfoEXT    resource{.type = writable ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage};
    resource.data.pImage = &image_info;
    m_Device.writeResourceDescriptorsEXT(
        resource,
        vk::HostAddressRangeEXT{
            .address = m_Heaps[0].mapped + m_Offsets[kind] + index * m_DescriptorSizes[kind],
            .size    = m_DescriptorSizes[kind],
        });
}

void VulkanDescriptorHeap::Write(std::uint32_t index, const Sampler& sampler) {
    m_Device.writeSamplerDescriptorsEXT(
        to_vk_sampler_create_info(sampler.GetDesc()),
        vk::HostAddressRangeEXT{
            .address = m_Heaps[1].mapped + index * m_DescriptorSizes[3],
            .size    = m_DescriptorSizes[3],
        });
}

void VulkanDescriptorHeap::Bind(const vk::raii::CommandBuffer& command_buffer, vk::PipelineBindPoint) const {
    command_buffer.bindResourceHeapEXT(m_Heaps[0].bind_info);
    command_buffer.bindSamplerHeapEXT(m_Heaps[1].bind_info);
}

void VulkanDescriptorHeap::PushMetaInfo(const vk::raii::CommandBuffer& command_buffer, const BindlessMetaInfo& info) const {
    command_buffer.pushDataEXT({.offset = 0, .data = {.address = &info, .size = sizeof(info)}});
}

VulkanDescriptorIndexing::VulkanDescriptorIndexing(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device)
    : m_Device(device) {
    constexpr auto& counts      = vulkan_bindless_descriptor_counts;
    constexpr auto& bindings    = vulkan_bindless_descriptor_bindings;
    const auto      properties  = physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorIndexingProperties>();
    const auto&     limits      = properties.get<vk::PhysicalDeviceDescriptorIndexingProperties>();
    const auto&     core_limits = properties.get<vk::PhysicalDeviceProperties2>().properties.limits;
    m_MaxStorageBufferRange     = core_limits.maxStorageBufferRange;
    constexpr auto total        = max_storage_descriptors + max_sampled_image_descriptors + max_storage_image_descriptors + max_sampler_descriptors;
    if (core_limits.maxPushConstantsSize < sizeof(BindlessMetaInfo) ||
        limits.maxDescriptorSetUpdateAfterBindStorageBuffers < max_storage_descriptors ||
        limits.maxPerStageDescriptorUpdateAfterBindStorageBuffers < max_storage_descriptors ||
        limits.maxDescriptorSetUpdateAfterBindSampledImages < max_sampled_image_descriptors ||
        limits.maxPerStageDescriptorUpdateAfterBindSampledImages < max_sampled_image_descriptors ||
        limits.maxDescriptorSetUpdateAfterBindStorageImages < max_storage_image_descriptors ||
        limits.maxPerStageDescriptorUpdateAfterBindStorageImages < max_storage_image_descriptors ||
        limits.maxDescriptorSetUpdateAfterBindSamplers < max_sampler_descriptors ||
        limits.maxPerStageDescriptorUpdateAfterBindSamplers < max_sampler_descriptors ||
        limits.maxPerStageUpdateAfterBindResources < total ||
        limits.maxUpdateAfterBindDescriptorsInAllPools < total) {
        throw std::runtime_error("Vulkan descriptor indexing capacity exceeds device limits");
    }
    constexpr std::array                          types{vk::DescriptorType::eStorageBuffer, vk::DescriptorType::eSampledImage, vk::DescriptorType::eStorageImage, vk::DescriptorType::eSampler};
    std::array<vk::DescriptorSetLayoutBinding, 4> layout_bindings;
    std::array<vk::DescriptorPoolSize, 4>         pool_sizes;
    std::array<vk::DescriptorBindingFlags, 4>     binding_flags;
    for (std::size_t kind = 0; kind < counts.size(); ++kind) {
        layout_bindings[kind] = {.binding = bindings[kind], .descriptorType = types[kind], .descriptorCount = counts[kind], .stageFlags = vk::ShaderStageFlagBits::eAll};
        pool_sizes[kind]      = {.type = types[kind], .descriptorCount = counts[kind]};
        binding_flags[kind]   = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;
    }
    const vk::DescriptorSetLayoutBindingFlagsCreateInfo flags{.bindingCount = 4, .pBindingFlags = binding_flags.data()};
    m_SetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
        device,
        vk::DescriptorSetLayoutCreateInfo{
            .pNext        = &flags,
            .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = 4,
            .pBindings    = layout_bindings.data(),
        });
    const vk::DescriptorSetLayout layout = **m_SetLayout;
    const vk::PushConstantRange   push_range{.stageFlags = vk::ShaderStageFlagBits::eAll, .offset = 0, .size = sizeof(BindlessMetaInfo)};
    m_PipelineLayout = std::make_unique<vk::raii::PipelineLayout>(
        device,
        vk::PipelineLayoutCreateInfo{
            .setLayoutCount         = 1,
            .pSetLayouts            = &layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push_range,
        });
    m_DescriptorPool = std::make_unique<vk::raii::DescriptorPool>(
        device,
        vk::DescriptorPoolCreateInfo{
            .flags         = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets       = 1,
            .poolSizeCount = 4,
            .pPoolSizes    = pool_sizes.data(),
        });
    auto sets       = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        .descriptorPool     = **m_DescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &layout,
    });
    m_DescriptorSet = std::make_unique<vk::raii::DescriptorSet>(std::move(sets.front()));
}

void VulkanDescriptorIndexing::Write(std::uint32_t index, const GPUBufferViewDesc& desc, std::uint64_t size) {
    if (size > m_MaxStorageBufferRange) throw std::invalid_argument("Storage buffer view exceeds Vulkan maxStorageBufferRange");
    const vk::DescriptorBufferInfo buffer_info{.buffer = **std::static_pointer_cast<VulkanBuffer>(desc.buffer)->buffer, .offset = desc.offset, .range = size};
    m_Device.updateDescriptorSets(
        vk::WriteDescriptorSet{
            .dstSet          = **m_DescriptorSet,
            .dstBinding      = 7,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo     = &buffer_info,
        },
        {});
}

void VulkanDescriptorIndexing::Write(std::uint32_t index, const TextureView& view) {
    const auto&                   desc     = view.GetDesc();
    const bool                    writable = desc.type == TextureViewType::ShaderWrite;
    const vk::DescriptorImageInfo image_info{
        .imageView   = *static_cast<const VulkanTextureView&>(view).image_view.value(),
        .imageLayout = writable ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    m_Device.updateDescriptorSets(
        vk::WriteDescriptorSet{
            .dstSet          = **m_DescriptorSet,
            .dstBinding      = writable ? 3u : 2u,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType  = writable ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage,
            .pImageInfo      = &image_info,
        },
        {});
}

void VulkanDescriptorIndexing::Write(std::uint32_t index, const Sampler& sampler) {
    const vk::DescriptorImageInfo sampler_info{.sampler = **static_cast<const VulkanSampler&>(sampler).sampler};
    m_Device.updateDescriptorSets(
        vk::WriteDescriptorSet{
            .dstSet          = **m_DescriptorSet,
            .dstBinding      = 0,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eSampler,
            .pImageInfo      = &sampler_info,
        },
        {});
}

void VulkanDescriptorIndexing::Bind(const vk::raii::CommandBuffer& command_buffer, vk::PipelineBindPoint bind_point) const {
    command_buffer.bindDescriptorSets(bind_point, **m_PipelineLayout, 0, **m_DescriptorSet, {});
}

void VulkanDescriptorIndexing::PushMetaInfo(const vk::raii::CommandBuffer& command_buffer, const BindlessMetaInfo& info) const {
    command_buffer.pushConstants<BindlessMetaInfo>(**m_PipelineLayout, vk::ShaderStageFlagBits::eAll, 0, info);
}

VulkanBindlessUtils::VulkanBindlessUtils(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, VmaAllocator allocator, bool descriptor_heap, std::string_view name)
    : BindlessUtils(name), m_Device(device) {
    if (descriptor_heap)
        m_Backend = std::make_unique<VulkanDescriptorHeap>(device, physical_device, allocator);
    else
        m_Backend = std::make_unique<VulkanDescriptorIndexing>(device, physical_device);

    for (std::size_t kind = 0; kind < vulkan_bindless_descriptor_counts.size(); ++kind) {
        auto&      pool  = m_BindlessHandlePools[kind].pool;
        const auto count = vulkan_bindless_descriptor_counts[kind];
        pool.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) pool.emplace_back(BindlessHandle{.index = index});
    }
}

VulkanBindlessUtils::~VulkanBindlessUtils() = default;

void VulkanBindlessUtils::Bind(const vk::raii::CommandBuffer& command_buffer, vk::PipelineBindPoint bind_point) const {
    std::visit([&](const auto& backend) { backend->Bind(command_buffer, bind_point); }, m_Backend);
}

void VulkanBindlessUtils::PushMetaInfo(const vk::raii::CommandBuffer& command_buffer, const BindlessMetaInfo& info) const {
    std::visit([&](const auto& backend) { backend->PushMetaInfo(command_buffer, info); }, m_Backend);
}

void VulkanBindlessUtils::ConfigureShader(vk::PipelineShaderStageCreateInfo& info) const {
    std::visit([&](const auto& backend) { backend->ConfigureShader(info); }, m_Backend);
}

auto VulkanBindlessUtils::CreatePipeline(vk::GraphicsPipelineCreateInfo info, const vk::AllocationCallbacks& callbacks) const -> std::unique_ptr<vk::raii::Pipeline> {
    return std::visit([&](const auto& backend) { return backend->CreatePipeline(info, callbacks); }, m_Backend);
}

auto VulkanBindlessUtils::CreatePipeline(vk::ComputePipelineCreateInfo info, const vk::AllocationCallbacks& callbacks) const -> std::unique_ptr<vk::raii::Pipeline> {
    return std::visit([&](const auto& backend) { return backend->CreatePipeline(info, callbacks); }, m_Backend);
}

void VulkanBindlessUtils::Write(std::uint32_t index, const GPUBufferViewDesc& desc, std::uint64_t size) {
    std::visit([&](const auto& backend) { backend->Write(index, desc, size); }, m_Backend);
}

void VulkanBindlessUtils::Write(std::uint32_t index, const TextureView& view) {
    std::visit([&](const auto& backend) { backend->Write(index, view); }, m_Backend);
}

void VulkanBindlessUtils::Write(std::uint32_t index, const Sampler& sampler) {
    std::visit([&](const auto& backend) { backend->Write(index, sampler); }, m_Backend);
}

}  // namespace hitagi::gfx
