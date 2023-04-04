#include "vk_resource.hpp"
#include "vk_device.hpp"
#include "vk_bindless.hpp"
#include "vk_utils.hpp"

#include <hitagi/utils/flags.hpp>

#include <fmt/color.h>
#include <spdlog/logger.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <spirv_reflect.h>

#include <numeric>

namespace hitagi::gfx {

VulkanBuffer::VulkanBuffer(VulkanDevice& device, GPUBufferDesc desc, std::span<const std::byte> initial_data) : GPUBuffer(device, desc) {
    const auto logger = device.GetLogger();

    logger->trace("Create buffer({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        const vk::BufferCreateInfo buffer_create_info = {
            .size  = m_Desc.element_size * m_Desc.element_count,
            .usage = to_vk_buffer_usage(m_Desc.usages),
        };
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Index)) {
            if (m_Desc.element_size != sizeof(std::uint16_t) && m_Desc.element_size != sizeof(std::uint32_t))
                logger->warn("Index buffer element size must be 16 bits or 32 bits");
        }

        buffer = std::make_unique<vk::raii::Buffer>(device.GetDevice(), buffer_create_info, device.GetCustomAllocator());
    }

    logger->trace("Allocate buffer({}) memory", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
    {
        VmaAllocationCreateInfo allocation_create_info{};
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapRead) ||
            utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::MapWrite)) {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

            if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Vertex) ||
                utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Index) ||
                utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Constant)) {
                allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
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
                              device.GetVmaAllocator(),
                              **buffer,
                              &allocation_create_info,
                              &allocation,
                              &allocation_info)) {
            auto error_message = fmt::format("failed to allocate memory for buffer({})", fmt::styled(desc.name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        vmaBindBufferMemory(device.GetVmaAllocator(), allocation, **buffer);

        logger->trace("Map buffer({}) memory", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapRead) ||
            utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            mapped_ptr = static_cast<std::byte*>(allocation_info.pMappedData);
        }
    }

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to buffer({})", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
        if (initial_data.size() > desc.element_size * desc.element_count) {
            logger->warn(
                "the initial data size({}) is larger than gpu buffer({}) size({}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                fmt::styled(desc.element_size * desc.element_count, fmt::fg(fmt::color::green)),
                fmt::styled(desc.name, fmt::fg(fmt::color::green)));
        }
        if (mapped_ptr != nullptr && utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            std::memcpy(mapped_ptr, initial_data.data(), std::min(initial_data.size(), desc.element_size * desc.element_count));
        } else if (utils::has_flag(desc.usages, GPUBufferUsageFlags::CopyDst)) {
            logger->trace("Using stage buffer to initial...");
            VulkanBuffer staging_buffer(
                device, GPUBufferDesc{
                            .name          = fmt::format("{}_staging", desc.name),
                            .element_size  = desc.element_size * desc.element_count,
                            .element_count = 1,
                            .usages        = GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::MapWrite,
                        },
                initial_data);

            auto  context    = device.CreateCopyContext("StageBufferToGPUBuffer");
            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);

            context->Begin();
            context->CopyBuffer(staging_buffer, 0, *this, 0, desc.element_size * desc.element_count);
            context->End();
            copy_queue.Submit({context.get()});

            // improve me
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "the gpu buffer({}) can not initialize with staging buffer without {}, the actual flags are {}",
                fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                fmt::styled(magic_enum::enum_flags_name(GPUBufferUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }

    create_vk_debug_object_info(*buffer, m_Name, device.GetDevice());
}

VulkanBuffer::~VulkanBuffer() {
    if (allocation) vmaFreeMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
}

auto VulkanBuffer::GetMappedPtr() const noexcept -> std::byte* {
    return mapped_ptr;
}

VulkanImage::VulkanImage(VulkanDevice& device, TextureDesc desc, std::span<const std::byte> initial_data) : Texture(device, desc) {
    const auto logger = device.GetLogger();

    logger->trace("Create Texture({})...", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        if (m_Desc.width == 0 || m_Desc.height == 0 || m_Desc.depth == 0) {
            const auto error_message = fmt::format(
                "the texture({}) size({} x {} x {}) can not be zero!",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.width, fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.height, fmt::fg(fmt::color::red)),
                fmt::styled(m_Desc.depth, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }

        image        = vk::raii::Image(device.GetDevice(), to_vk_image_create_info(m_Desc), device.GetCustomAllocator());
        image_handle = **image;
        create_vk_debug_object_info(image.value(), m_Name, device.GetDevice());
    }

    logger->trace("Allocate memory for Texture({})...", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        const VmaAllocationCreateInfo vma_alloc_create_info{
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        if (VK_SUCCESS != vmaAllocateMemoryForImage(
                              device.GetVmaAllocator(),
                              *image.value(),
                              &vma_alloc_create_info,
                              &allocation,
                              nullptr)) {
            auto error_message = fmt::format("failed to allocate memory for texture({})", fmt::styled(desc.name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        vmaBindImageMemory(device.GetVmaAllocator(), allocation, **image);
    }

    logger->trace("Create TextureView({})...", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        vk::ImageViewType image_view_type;
        if (m_Desc.height == 1 && m_Desc.depth == 1) {
            image_view_type = m_Desc.array_size == 1 ? vk::ImageViewType::e1D : vk::ImageViewType::e1DArray;
        } else if (m_Desc.depth == 1) {
            image_view_type = m_Desc.array_size == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
        } else {
            image_view_type = vk::ImageViewType::e3D;
        }

        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::Cube) ||
            utils::has_flag(m_Desc.usages, TextureUsageFlags::CubeArray)) {
            if (image_view_type != vk::ImageViewType::e2DArray) {
                const auto error_message = fmt::format(
                    "the texture({}) can not be cube texture because the texture view type is not 2D texture array!",
                    fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
                logger->error(error_message);
                throw std::invalid_argument(error_message);
            }
        }
        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::Cube | TextureUsageFlags::CubeArray)) {
            const auto error_message = fmt::format(
                "the texture({}) can not be cube texture and cube array at same time!",
                fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::Cube)) {
            image_view_type = vk::ImageViewType::eCube;
        }
        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::CubeArray)) {
            image_view_type = vk::ImageViewType::eCubeArray;
        }

        const vk::ImageViewCreateInfo image_view_create_info{
            .image    = **image,
            .viewType = image_view_type,
            .format   = to_vk_format(m_Desc.format),
            .subresourceRange =
                vk::ImageSubresourceRange{
                    .aspectMask     = to_vk_image_aspect(m_Desc.usages),
                    .baseMipLevel   = 0,
                    .levelCount     = m_Desc.mip_levels,
                    .baseArrayLayer = 0,
                    .layerCount     = m_Desc.array_size,
                },
        };

        image_view = vk::raii::ImageView(device.GetDevice(), image_view_create_info, device.GetCustomAllocator());
        create_vk_debug_object_info(image_view.value(), m_Name, device.GetDevice());
    }

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to texture({})", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::CopyDst)) {
            VulkanBuffer staging_buffer(
                device, GPUBufferDesc{
                            .name          = fmt::format("{}_staging", m_Desc.name),
                            .element_size  = initial_data.size_bytes(),
                            .element_count = 1,
                            .usages        = GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::MapWrite,
                        },
                initial_data);

            auto  context    = device.CreateCopyContext("StageBufferToTexture");
            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);

            context->Begin();
            context->ResourceBarrier(
                {}, {},
                {
                    TextureBarrier{
                        .src_access       = BarrierAccess::Unkown,
                        .dst_access       = BarrierAccess::CopyDst,
                        .src_stage        = PipelineStage::None,
                        .dst_stage        = PipelineStage::Copy,
                        .src_layout       = TextureLayout::Unkown,
                        .dst_layout       = TextureLayout::CopyDst,
                        .texture          = *this,
                        .base_mip_level   = 0,
                        .level_count      = m_Desc.mip_levels,
                        .base_array_layer = 0,
                        .layer_count      = m_Desc.array_size,
                    },
                });

            context->CopyBufferToTexture(
                staging_buffer,
                0,
                *this,
                {0, 0, 0},
                {m_Desc.width, m_Desc.height, m_Desc.depth},
                0, 0, m_Desc.array_size);

            context->ResourceBarrier(
                {}, {},
                {
                    TextureBarrier{
                        .src_access       = BarrierAccess::CopyDst,
                        .dst_access       = utils::has_flag(m_Desc.usages, TextureUsageFlags::DSV)
                                                ? BarrierAccess::DepthStencilRead
                                                : BarrierAccess::ShaderRead,
                        .src_stage        = PipelineStage::Copy,
                        .dst_stage        = PipelineStage::All,
                        .src_layout       = TextureLayout::CopyDst,
                        .dst_layout       = utils::has_flag(m_Desc.usages, TextureUsageFlags::DSV)
                                                ? TextureLayout::DepthStencilRead
                                                : TextureLayout::ShaderRead,
                        .texture          = *this,
                        .base_mip_level   = 0,
                        .level_count      = m_Desc.mip_levels,
                        .base_array_layer = 0,
                        .layer_count      = m_Desc.array_size,
                    },
                });

            context->End();
            copy_queue.Submit({context.get()});

            // improve me
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "the texture({}) can not initialize with staging buffer without {}, the actual flags are {}",
                fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                fmt::styled(magic_enum::enum_flags_name(TextureUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

VulkanImage::VulkanImage(const VulkanSwapChain& swap_chian, std::uint32_t index)
    : Texture(swap_chian.GetDevice(),
              {
                  .name        = fmt::format("{}-texture-{}", swap_chian.GetName(), index),
                  .width       = swap_chian.GetWidth(),
                  .height      = swap_chian.GetHeight(),
                  .format      = swap_chian.GetFormat(),
                  .clear_value = swap_chian.GetDesc().clear_value,
                  .usages      = TextureUsageFlags::RTV,
              }) {
    const auto& vk_device = static_cast<VulkanDevice&>(m_Device);
    const auto  _images   = swap_chian.GetVkSwapChain().getImages();

    image_view = vk::raii::ImageView(
        vk_device.GetDevice(),
        {
            .image            = _images.at(index),
            .viewType         = vk::ImageViewType::e2D,
            .format           = to_vk_format(m_Desc.format),
            .subresourceRange = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        },
        vk_device.GetCustomAllocator());
    image_handle = _images.at(index);

    create_vk_debug_object_info(image_view.value(), m_Name, vk_device.GetDevice());
}

VulkanImage::~VulkanImage() {
    if (allocation) vmaFreeMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
}

VulkanSampler::VulkanSampler(VulkanDevice& device, SamplerDesc desc) : Sampler(device, desc) {
    sampler = std::make_unique<vk::raii::Sampler>(device.GetDevice(), to_vk_sampler_create_info(m_Desc), device.GetCustomAllocator());
    create_vk_debug_object_info(*sampler, m_Name, device.GetDevice());
}

VulkanSwapChain::SemaphorePair::SemaphorePair(VulkanDevice& device, std::string_view name)
    : image_available(std::make_shared<vk::raii::Semaphore>(device.GetDevice(), vk::SemaphoreCreateInfo{}, device.GetCustomAllocator())),
      presentable(std::make_shared<vk::raii::Semaphore>(device.GetDevice(), vk::SemaphoreCreateInfo{}, device.GetCustomAllocator())) {
    create_vk_debug_object_info(*image_available, fmt::format("{}-image-available", name), device.GetDevice());
    create_vk_debug_object_info(*presentable, fmt::format("{}-presentable", name), device.GetDevice());
}

VulkanSwapChain::VulkanSwapChain(VulkanDevice& device, SwapChainDesc desc)
    : SwapChain(device, desc), m_SemaphorePair(device, m_Name)

{
    auto window_size = math::vec2u{};

    switch (desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            auto h_wnd = static_cast<HWND>(desc.window.ptr);

            RECT rect;
            GetClientRect(static_cast<HWND>(desc.window.ptr), &rect);
            window_size.x = rect.right - rect.left;
            window_size.y = rect.bottom - rect.top;

            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandle(nullptr),
                .hwnd      = h_wnd,
            };
            m_Surface = std::make_unique<vk::raii::SurfaceKHR>(device.GetInstance(), surface_create_info, device.GetCustomAllocator());
        } break;
#endif
        case utils::Window::Type::SDL2: {
            auto sdl_window = static_cast<SDL_Window*>(desc.window.ptr);
            window_size     = get_sdl2_drawable_size(sdl_window);

            SDL_SysWMinfo wm_info;
            SDL_VERSION(&wm_info.version);
            if (!SDL_GetWindowWMInfo(sdl_window, &wm_info)) {
                const auto error_message = fmt::format("SDL_GetWindowWMInfo failed: {}", SDL_GetError());
                device.GetLogger()->error(error_message);
                throw std::runtime_error(error_message);
            }
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            assert(wm_info.subsystem == SDL_SYSWM_WINDOWS);
            HWND                          h_wnd = wm_info.info.win.window;
            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandle(nullptr),
                .hwnd      = h_wnd,
            };
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            assert(wm_info.subsystem == SDL_SYSWM_WAYLAND);
            vk::WaylandSurfaceCreateInfoKHR surface_create_info{
                .display = wm_info.info.wl.display,
                .surface = wm_info.info.wl.surface,
            };
#endif
            m_Surface = std::make_unique<vk::raii::SurfaceKHR>(device.GetInstance(), surface_create_info, device.GetCustomAllocator());
        } break;
    }

    CreateSwapChain();
    CreateImageViews();
}

auto VulkanSwapChain::AcquireImageForRendering() -> VulkanImage& {
    if (m_CurrentIndex != -1) {
        return m_Images[m_CurrentIndex];
    }

    auto [result, index] = m_SwapChain->acquireNextImage(
        std::numeric_limits<std::uint64_t>::max(),
        **m_SemaphorePair.image_available);

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire next image");
    }

    m_CurrentIndex = index;

    return m_Images[m_CurrentIndex];
}

void VulkanSwapChain::Present() {
    auto& vk_device = static_cast<VulkanDevice&>(m_Device);
    auto& queue     = static_cast<VulkanCommandQueue&>(vk_device.GetCommandQueue(CommandType::Graphics)).GetVkQueue();

    std::uint32_t index = m_CurrentIndex;
    m_CurrentIndex      = -1;

    auto result = queue.presentKHR(vk::PresentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &(**m_SemaphorePair.presentable),
        .swapchainCount     = 1,
        .pSwapchains        = &(**m_SwapChain),
        .pImageIndices      = &index,
    });

    if (result != vk::Result::eSuccess) {
        vk_device.GetLogger()->error("failed to present swap chain image");
    }
}

void VulkanSwapChain::Resize() {
    m_Device.WaitIdle();
    CreateSwapChain();
    CreateImageViews();
}

void VulkanSwapChain::CreateSwapChain() {
    m_SwapChain = nullptr;

    auto& vk_device = static_cast<VulkanDevice&>(m_Device);

    math::vec2u window_size;
    switch (m_Desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            RECT rect;
            GetClientRect(static_cast<HWND>(m_Desc.window.ptr), &rect);
            window_size.x = rect.right - rect.left;
            window_size.y = rect.bottom - rect.top;
        } break;
#endif
        case utils::Window::Type::SDL2:
            window_size = get_sdl2_drawable_size(static_cast<SDL_Window*>(m_Desc.window.ptr));
            break;
    }

    // we use graphics queue as present queue
    const auto& graphics_queue  = static_cast<VulkanCommandQueue&>(vk_device.GetCommandQueue(CommandType::Graphics));
    const auto& physical_device = vk_device.GetPhysicalDevice();
    if (!physical_device.getSurfaceSupportKHR(graphics_queue.GetFamilyIndex(), **m_Surface)) {
        throw std::runtime_error(fmt::format(
            "The graphics queue({}) of physical device({}) can not support surface(window.ptr: {})",
            graphics_queue.GetFamilyIndex(),
            physical_device.getProperties().deviceName,
            m_Desc.window.ptr));
    }

    const auto surface_capabilities    = physical_device.getSurfaceCapabilitiesKHR(**m_Surface);
    const auto supported_formats       = physical_device.getSurfaceFormatsKHR(**m_Surface);
    const auto supported_present_modes = physical_device.getSurfacePresentModesKHR(**m_Surface);

    m_Format = from_vk_format(supported_formats.front().format);

    if (std::find_if(supported_present_modes.begin(), supported_present_modes.end(), [](const auto& present_mode) {
            return present_mode == vk::PresentModeKHR::eFifo;
        }) == supported_present_modes.end()) {
        throw std::runtime_error(fmt::format(
            "The physical device({}) can not support surface(window.ptr: {}) with present mode: VK_PRESENT_MODE_FIFO_KHR",
            physical_device.getProperties().deviceName,
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
        .imageExtent      = vk::Extent2D{m_Size.x, m_Size.y},
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform     = preTransform,
        .compositeAlpha   = composite_alpha,
        .presentMode      = vk::PresentModeKHR::eFifo,
        .clipped          = true,
    };

    // We use graphics queue as present queue, so we do not care the ownership of images
    m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(vk_device.GetDevice(), swapchain_create_info, vk_device.GetCustomAllocator());

    create_vk_debug_object_info(*m_SwapChain, m_Name, vk_device.GetDevice());
}

void VulkanSwapChain::CreateImageViews() {
    m_Images.clear();
    for (std::uint32_t index = 0; index < m_NumImages; index++) {
        m_Images.emplace_back(*this, index);
    }
}

VulkanShader::VulkanShader(VulkanDevice& device, ShaderDesc desc, std::span<const std::byte> _binary_program)
    : Shader(device, std::move(desc)),
      binary_program(_binary_program.empty() ? device.GetShaderCompiler().CompileToSPIRV(m_Desc) : _binary_program),
      shader(
          device.GetDevice(),
          {
              .codeSize = binary_program.GetDataSize(),
              .pCode    = reinterpret_cast<const std::uint32_t*>(binary_program.GetData()),
          },
          device.GetCustomAllocator())

{
    create_vk_debug_object_info(shader, m_Name, device.GetDevice());
}

auto VulkanShader::GetSPIRVData() const noexcept -> std::span<const std::byte> {
    return binary_program.Span<const std::byte>();
}

VulkanRenderPipeline::VulkanRenderPipeline(VulkanDevice& device, RenderPipelineDesc desc) : RenderPipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();

    if (std::find_if(
            m_Desc.shaders.begin(), m_Desc.shaders.end(),
            [](const auto& _shader) {
                auto shader = _shader.lock();
                return shader && shader->GetDesc().type == ShaderType::Vertex;
            }) == m_Desc.shaders.end()) {
        const auto error_message = fmt::format(
            "Vertex shader is not specified for pipeline({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    std::pmr::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos;
    std::transform(
        m_Desc.shaders.begin(), m_Desc.shaders.end(), std::back_inserter(shader_stage_create_infos),
        [](const auto& _shader) {
            auto shader = _shader.lock();
            return vk::PipelineShaderStageCreateInfo{
                .stage  = to_vk_shader_stage(shader->GetDesc().type),
                .module = *std::static_pointer_cast<VulkanShader>(shader)->shader,
                .pName  = shader->GetDesc().entry.data(),
            };
        });

    const auto assembly_state = to_vk_assembly_state(m_Desc.assembly_state);

    std::pmr::vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions;
    std::transform(
        m_Desc.vertex_input_layout.begin(), m_Desc.vertex_input_layout.end(),
        std::back_inserter(vertex_input_attribute_descriptions),
        [&](const auto& attribute) {
            return vk::VertexInputAttributeDescription{
                .location = static_cast<std::uint32_t>(vertex_input_attribute_descriptions.size()),
                .binding  = attribute.binding,
                .format   = to_vk_format(attribute.format),
                .offset   = static_cast<std::uint32_t>(attribute.offset),
            };
        });

    std::pmr::vector<vk::VertexInputBindingDescription> vertex_input_binding_descriptions;
    // Then sort all semantic strings according to declaration and assign Location numbers sequentially to the corresponding SPIR-V variables.
    // https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#implicit-location-number-assignment
    {
        std::transform(
            m_Desc.vertex_input_layout.begin(), m_Desc.vertex_input_layout.end(),
            std::back_inserter(vertex_input_binding_descriptions),
            [](const auto& attribute) {
                return vk::VertexInputBindingDescription{
                    .binding   = attribute.binding,
                    .stride    = static_cast<std::uint32_t>(attribute.stride),
                    .inputRate = attribute.per_instance
                                     ? vk::VertexInputRate::eInstance
                                     : vk::VertexInputRate::eVertex,
                };
            });
        auto iter = std::unique(vertex_input_binding_descriptions.begin(), vertex_input_binding_descriptions.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.binding == rhs.binding && (lhs.stride != rhs.stride || lhs.inputRate != rhs.inputRate))
                throw std::invalid_argument("Vertex input binding description is not same");
            return lhs.binding == rhs.binding;
        });
        vertex_input_binding_descriptions.erase(iter, vertex_input_binding_descriptions.end());
    }

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

    const auto& pipeline_layout = *static_cast<VulkanBindlessUtils&>(device.GetBindlessUtils()).pipeline_layout;

    logger->trace("Create render pipeline({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        const vk::StructureChain pipeline_create_info = {
            vk::GraphicsPipelineCreateInfo{
                .stageCount          = static_cast<std::uint32_t>(shader_stage_create_infos.size()),
                .pStages             = shader_stage_create_infos.data(),
                .pVertexInputState   = &vertex_state,
                .pInputAssemblyState = &assembly_state,
                .pViewportState      = &viewport_state,
                .pRasterizationState = &rasterization_state,
                .pDepthStencilState  = &depth_stencil_state,
                .pColorBlendState    = &blend_state,
                .pDynamicState       = &dynamic_state_create_info,
                .layout              = *pipeline_layout,
                .renderPass          = nullptr,
            },
            vk::PipelineRenderingCreateInfo{
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &render_format,
            },
        };

        pipeline = std::make_unique<vk::raii::Pipeline>(device.GetDevice(), nullptr, pipeline_create_info.get(), device.GetCustomAllocator());

        switch (pipeline->getConstructorSuccessCode()) {
            case vk::Result::eSuccess:
                break;
            case vk::Result::ePipelineCompileRequired:
                throw std::runtime_error("Pipeline compilation is required");
            default:
                throw std::runtime_error("Failed to create pipeline");
        }
        create_vk_debug_object_info(*pipeline, m_Name, device.GetDevice());
    }
}

VulkanComputePipeline::VulkanComputePipeline(VulkanDevice& device, ComputePipelineDesc desc) : ComputePipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();

    auto compute_shader = std::static_pointer_cast<VulkanShader>(m_Desc.cs.lock());

    if (compute_shader == nullptr) {
        const auto error_message = fmt::format("Compute shader is not specified for pipeline({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    const vk::PipelineShaderStageCreateInfo shader_stage_create_info{
        .stage  = vk::ShaderStageFlagBits::eCompute,
        .module = *compute_shader->shader,
        .pName  = compute_shader->GetDesc().entry.data(),
    };

    const auto& pipeline_layout = *static_cast<VulkanBindlessUtils&>(device.GetBindlessUtils()).pipeline_layout;

    logger->trace("Create compute pipeline({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        const vk::ComputePipelineCreateInfo pipeline_create_info{
            .stage  = shader_stage_create_info,
            .layout = *pipeline_layout,
        };

        pipeline = std::make_unique<vk::raii::Pipeline>(device.GetDevice(), nullptr, pipeline_create_info, device.GetCustomAllocator());

        switch (pipeline->getConstructorSuccessCode()) {
            case vk::Result::eSuccess:
                break;
            case vk::Result::ePipelineCompileRequired:
                throw std::runtime_error("Pipeline compilation is required");
            default:
                throw std::runtime_error("Failed to create pipeline");
        }
        create_vk_debug_object_info(*pipeline, m_Name, device.GetDevice());
    }
}

}  // namespace hitagi::gfx