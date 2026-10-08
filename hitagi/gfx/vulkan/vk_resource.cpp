module;
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <fmt/color.h>
#include <spdlog/logger.h>
#include <SDL3/SDL.h>
#include <spirv_reflect.h>

export module gfx.vulkan:resource;
import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
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
    VulkanBufferView(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc);
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

    VulkanSwapChain(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physical_device, const vk::raii::Queue& queue, std::uint32_t queue_family_index, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc);

    auto AcquireTextureForRendering() -> utils::optional_ref<Texture> final;

    inline auto GetWidth() const noexcept -> std::uint32_t final { return m_Size.x; };
    inline auto GetHeight() const noexcept -> std::uint32_t final { return m_Size.y; };
    inline auto GetFormat() const noexcept -> Format final { return m_Format; }

    void Present() final;
    void Resize() final;

    inline auto& GetVkSwapChain() const noexcept { return *m_SwapChain; }

    inline const auto& GetSemaphores() const noexcept { return m_CurrentSemaphores; }

private:
    const vk::raii::Device&         m_Device;
    const vk::AllocationCallbacks&  m_Callbacks;
    const vk::raii::PhysicalDevice& m_PhysicalDevice;
    const vk::raii::Queue&          m_Queue;
    std::uint32_t                   m_QueueFamilyIndex;
    std::shared_ptr<spdlog::logger> m_Logger;

    void CreateSwapChain();
    void CreateImageViews();

    std::unique_ptr<vk::raii::SurfaceKHR>      m_Surface;
    std::unique_ptr<vk::raii::SwapchainKHR>    m_SwapChain;
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
    VulkanShader(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const ShaderCompiler& compiler, ShaderDesc desc);

    auto GetSPIRVData() const noexcept -> std::span<const std::byte> final;

    core::Buffer           binary_program;
    vk::raii::ShaderModule shader;
};

struct VulkanRenderPipeline final : public RenderPipeline {
    VulkanRenderPipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::ShaderDescriptorSetAndBindingMappingInfoEXT& mapping_info, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

struct VulkanComputePipeline final : public ComputePipeline {
    VulkanComputePipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::ShaderDescriptorSetAndBindingMappingInfoEXT& mapping_info, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanBuffer::VulkanBuffer(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VmaAllocator allocator, std::shared_ptr<spdlog::logger> logger, GPUBufferDesc desc, std::span<const std::byte> initial_data) : GPUBuffer(std::move(desc)), m_Allocator(allocator), m_Logger(std::move(logger)) {
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

VulkanBufferView::VulkanBufferView(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc) : GPUBufferView(bindings, storage_requirements, std::move(desc)) {
    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<VulkanBindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(device.getBufferAddress({.buffer = **static_cast<VulkanBuffer&>(*m_Desc.buffer).buffer}), m_Desc, Size());
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
        if (const auto result = vmaInvalidateAllocation(m_Allocator, allocation, 0, VK_WHOLE_SIZE);
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
        if (const auto result = vmaFlushAllocation(m_Allocator, allocation, 0, VK_WHOLE_SIZE);
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

VulkanImage::VulkanImage(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VmaAllocator allocator, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data) : Texture(std::move(desc)), m_Allocator(allocator), native_device(*device) {
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
    const auto  _images   = _swap_chian.GetVkSwapChain().getImages();
    create_vk_debug_object_info(_images.at(index), GetName(), device);

    image_handle = _images.at(index);
}

VulkanTextureView::VulkanTextureView(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc) : TextureView(bindings, std::move(desc)) {
    const auto fail   = [&](std::string message) {
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

    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<VulkanBindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(static_cast<VulkanImage&>(*m_Desc.texture).image_handle, m_Desc);
}

VulkanImage::~VulkanImage() {
    if (allocation) vmaFreeMemory(m_Allocator, allocation);
}

VulkanSampler::VulkanSampler(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, VulkanBindlessUtils& bindings, SamplerDesc desc) : Sampler(bindings, std::move(desc)) {
    sampler = std::make_unique<vk::raii::Sampler>(device, to_vk_sampler_create_info(m_Desc), callbacks);
    create_vk_debug_object_info(*sampler, GetName(), device);
    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<VulkanBindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(m_Desc);
}

VulkanSwapChain::SemaphorePair::SemaphorePair(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, std::string_view name)
    : image_available(std::make_shared<vk::raii::Semaphore>(device, vk::SemaphoreCreateInfo{}, callbacks)),
      presentable(std::make_shared<vk::raii::Semaphore>(device, vk::SemaphoreCreateInfo{}, callbacks)) {
    create_vk_debug_object_info(*image_available, std::format("{}-image-available", name), device);
    create_vk_debug_object_info(*presentable, std::format("{}-presentable", name), device);
}

VulkanSwapChain::VulkanSwapChain(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physical_device, const vk::raii::Queue& queue, std::uint32_t queue_family_index, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc)
    : SwapChain(std::move(desc)), m_Device(device), m_Callbacks(callbacks), m_PhysicalDevice(physical_device), m_Queue(queue), m_QueueFamilyIndex(queue_family_index), m_Logger(std::move(logger)) {
    switch (desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            auto                          h_wnd = static_cast<HWND>(desc.window.ptr);
            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandle(nullptr),
                .hwnd      = h_wnd,
            };
            m_Surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface_create_info, callbacks);
        } break;
#endif
        case utils::Window::Type::SDL3: {
            auto sdl_window = static_cast<SDL_Window*>(desc.window.ptr);
            auto h_wnd      = reinterpret_cast<HWND>(
                SDL_GetPointerProperty(
                    SDL_GetWindowProperties(sdl_window),
                    SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                    nullptr));
            if (!h_wnd) {
                const auto error_message = std::format("SDL_GetWindowProperties failed: {}", SDL_GetError());
                m_Logger->error(error_message);
                throw std::runtime_error(error_message);
            }
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandle(nullptr),
                .hwnd      = h_wnd,
            };
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            assert((SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0));
            auto display = reinterpret_cast<struct wl_display*>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
            auto surface = reinterpret_cast<struct wl_surface*>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
            if (!display || !surface) {
                const auto error_message = std::format("SDL_GetPointerProperty failed: {}", SDL_GetError());
                m_Logger->error(error_message);
                throw std::runtime_error(error_message);
            }
            vk::WaylandSurfaceCreateInfoKHR surface_create_info{
                .display = display,
                .surface = surface,
            };
#endif
            m_Surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface_create_info, callbacks);
        } break;
    }

    CreateSwapChain();
    CreateImageViews();
}

auto VulkanSwapChain::AcquireTextureForRendering() -> utils::optional_ref<Texture> {
    if (m_SwapChain == nullptr) return {};

    if (m_CurrentIndex != -1) {
        return *m_Images[m_CurrentIndex];
    }

    if (m_SemaphorePairs.empty()) {
        return {};
    }

    auto& acquire_semaphores = m_SemaphorePairs[m_NextSemaphoreIndex];
    m_NextSemaphoreIndex     = (m_NextSemaphoreIndex + 1) % static_cast<std::uint32_t>(m_SemaphorePairs.size());

    auto [result, index] = m_SwapChain->acquireNextImage(
        std::numeric_limits<std::uint64_t>::max(),
        **acquire_semaphores.image_available);

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire next image");
    }

    m_CurrentIndex                      = static_cast<int>(index);
    m_CurrentSemaphores.image_available = acquire_semaphores.image_available;
    m_CurrentSemaphores.presentable     = m_SemaphorePairs[index].presentable;

    return *m_Images[m_CurrentIndex];
}

void VulkanSwapChain::Present() {
    if (!m_SwapChain || m_CurrentIndex == -1) return;

    const auto& queue = m_Queue;

    std::uint32_t index                 = static_cast<std::uint32_t>(m_CurrentIndex);
    auto          presentable_semaphore = m_CurrentSemaphores.presentable;
    m_CurrentIndex                      = -1;
    m_CurrentSemaphores                 = {};

    auto result = queue.presentKHR(vk::PresentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &(**presentable_semaphore),
        .swapchainCount     = 1,
        .pSwapchains        = &(**m_SwapChain),
        .pImageIndices      = &index,
    });

    if (result != vk::Result::eSuccess) {
        m_Logger->error("failed to present swap chain image");
    }
}

void VulkanSwapChain::Resize() {
    m_Device.waitIdle();
    m_CurrentIndex       = -1;
    m_NextSemaphoreIndex = 0;
    m_CurrentSemaphores  = {};
    CreateSwapChain();
    CreateImageViews();
}

void VulkanSwapChain::CreateSwapChain() {
    m_SwapChain = nullptr;


    math::vec2u window_size;
    switch (m_Desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            if (!IsWindow(static_cast<HWND>(m_Desc.window.ptr))) return;
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

    m_Format = from_vk_format(supported_formats.front().format);

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
        (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)    ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)        ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                                                                          : vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface          = **m_Surface,
        .minImageCount    = m_NumImages,
        .imageFormat      = to_vk_format(m_Format),
        .imageExtent      = vk::Extent2D{.width = m_Size.x, .height = m_Size.y},
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        .preTransform     = preTransform,
        .compositeAlpha   = composite_alpha,
        .presentMode      = vk::PresentModeKHR::eFifo,
        .clipped          = true,
    };

    // We use graphics queue as present queue, so we do not care the ownership of images
    m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(m_Device, swapchain_create_info, m_Callbacks);

    create_vk_debug_object_info(*m_SwapChain, GetName(), m_Device);
}

void VulkanSwapChain::CreateImageViews() {
    if (!m_SwapChain) return;

    m_Images.clear();
    m_SemaphorePairs.clear();
    for (std::uint32_t index = 0; index < m_NumImages; index++) {
        m_SemaphorePairs.emplace_back(m_Device, m_Callbacks, std::format("{}-frame-{}", GetName(), index));
        m_Images.emplace_back(std::make_unique<VulkanImage>(m_Device, *this, index));
    }
}

VulkanShader::VulkanShader(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const ShaderCompiler& compiler, ShaderDesc desc)
    : Shader(std::move(desc)),
      binary_program(compiler.CompileToSPIRV(m_Desc)),
      shader(
          device,
          {
              .codeSize = binary_program.GetDataSize(),
              .pCode    = reinterpret_cast<const std::uint32_t*>(binary_program.GetData()),
          },
          callbacks)

{
    create_vk_debug_object_info(shader, GetName(), device);
}

auto VulkanShader::GetSPIRVData() const noexcept -> std::span<const std::byte> {
    return binary_program.Span<const std::byte>();
}

VulkanRenderPipeline::VulkanRenderPipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::ShaderDescriptorSetAndBindingMappingInfoEXT& mapping_info, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) : RenderPipeline(std::move(desc)) {
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
        [&mapping_info](const auto& shader) {
            return vk::PipelineShaderStageCreateInfo{
                .pNext  = &mapping_info,
                .stage  = to_vk_shader_stage(shader->GetDesc().type),
                .module = *std::static_pointer_cast<VulkanShader>(shader)->shader,
                .pName  = shader->GetDesc().entry.data(),
            };
        });

    const auto assembly_state = to_vk_assembly_state(m_Desc.assembly_state);

    auto vertex_input_attribute_descriptions = m_Desc.vertex_input_layout |
                                               std::ranges::views::enumerate |
                                               std::ranges::views::transform([](const auto& pair) {
                                                   const auto& [location, attribute] = pair;
                                                   return vk::VertexInputAttributeDescription{
                                                       .location = static_cast<std::uint32_t>(location),
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
        const vk::StructureChain pipeline_create_info = {
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
                .layout              = nullptr,
                .renderPass          = nullptr,
            },
            vk::PipelineCreateFlags2CreateInfo{.flags = vk::PipelineCreateFlagBits2::eDescriptorHeapEXT},
            vk::PipelineRenderingCreateInfo{
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &render_format,
                .depthAttachmentFormat   = depth_format,
                .stencilAttachmentFormat = stencil_format,
            },
        };

        pipeline = std::make_unique<vk::raii::Pipeline>(device, nullptr, pipeline_create_info.get(), callbacks);

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

VulkanComputePipeline::VulkanComputePipeline(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, const vk::ShaderDescriptorSetAndBindingMappingInfoEXT& mapping_info, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) : ComputePipeline(std::move(desc)) {
    auto compute_shader = std::dynamic_pointer_cast<VulkanShader>(cs);

    if (compute_shader == nullptr) {
        const auto error_message = fmt::format("Compute shader is not specified for pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    const vk::PipelineShaderStageCreateInfo shader_stage_create_info{
        .pNext  = &mapping_info,
        .stage  = vk::ShaderStageFlagBits::eCompute,
        .module = *compute_shader->shader,
        .pName  = compute_shader->GetDesc().entry.data(),
    };

    logger->trace("Create compute pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        const vk::StructureChain pipeline_create_info{
            vk::ComputePipelineCreateInfo{.stage = shader_stage_create_info},
            vk::PipelineCreateFlags2CreateInfo{.flags = vk::PipelineCreateFlagBits2::eDescriptorHeapEXT},
        };

        pipeline = std::make_unique<vk::raii::Pipeline>(device, nullptr, pipeline_create_info.get(), callbacks);

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

}  // namespace hitagi::gfx
