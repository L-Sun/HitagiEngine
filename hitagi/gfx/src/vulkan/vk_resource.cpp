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
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/unique.hpp>
#include <range/v3/range/conversion.hpp>

namespace hitagi::gfx {

VulkanBuffer::VulkanBuffer(VulkanDevice& device, GPUBufferDesc desc, std::span<const std::byte> initial_data) : GPUBuffer(device, std::move(desc)) {
    const auto logger = device.GetLogger();

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
        if (utils::has_flag(m_Desc.usages, GPUBufferUsageFlags::Index)) {
            if (m_Desc.element_size != sizeof(std::uint16_t) && m_Desc.element_size != sizeof(std::uint32_t)) {
                const auto error_message = "Index buffer element size must be 16 bits or 32 bits";
                logger->error(error_message);
                throw std::invalid_argument(error_message);
            }
        }

        buffer = std::make_unique<vk::raii::Buffer>(device.GetDevice(), buffer_create_info, device.GetCustomAllocator());
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
            auto error_message = fmt::format("failed to allocate memory for buffer({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        vmaBindBufferMemory(device.GetVmaAllocator(), allocation, **buffer);
    }

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

        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            // Unlike DX12 where the buffer may be align to 256 bytes, Vulkan buffer is not align to 256 bytes.
            auto mapped_ptr = Map();
            std::memcpy(mapped_ptr, initial_data.data(), std::min(initial_data.size(), Size()));
            UnMap();
        } else if (utils::has_flag(desc.usages, GPUBufferUsageFlags::CopyDst)) {
            logger->trace("Using stage buffer to initial...");
            VulkanBuffer staging_buffer(
                device, GPUBufferDesc{
                            .name         = std::pmr::string(fmt::format("{}_staging", GetName())),
                            .element_size = Size(),
                            .usages       = GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::MapWrite,
                        },
                initial_data);

            auto  context    = device.CreateCopyContext("StageBufferToGPUBuffer");
            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);

            context->Begin();
            context->CopyBuffer(staging_buffer, 0, *this, 0, Size());
            context->End();
            copy_queue.Submit({{*context}});

            // improve me
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "Can not initialize gpu buffer({}) using upload heap without the flag {} or {}, the actual flags are {}",
                fmt::styled(GetName(), fmt::fg(fmt::color::green)),
                fmt::styled(GPUBufferUsageFlags::CopyDst, fmt::fg(fmt::color::green)),
                fmt::styled(GPUBufferUsageFlags::MapWrite, fmt::fg(fmt::color::green)),
                fmt::styled(m_Desc.usages, fmt::fg(fmt::color::red)));

            logger->error(error_message);
            vmaFreeMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
            throw std::invalid_argument(error_message);
        }
    }

    create_vk_debug_object_info(*buffer, GetName(), device.GetDevice());
}

VulkanBuffer::~VulkanBuffer() {
    if (allocation) vmaFreeMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
}

auto VulkanBuffer::Map() -> std::byte* {
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

    auto& vk_device = static_cast<VulkanDevice&>(m_Device);

    std::byte* mapped_ptr = nullptr;
    if (VK_SUCCESS != vmaMapMemory(vk_device.GetVmaAllocator(),
                                   allocation,
                                   reinterpret_cast<void**>(&mapped_ptr))) {
        const auto error_message = fmt::format(
            "failed to map GPU buffer({})",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    std::lock_guard lock(map_mutex);
    mapped_count++;
    return mapped_ptr;
}

void VulkanBuffer::UnMap() {
    std::lock_guard lock(map_mutex);

    if (mapped_count == 0) {
        const auto error_message = fmt::format(
            "Can not unmap GPU buffer({}) without map it!",
            fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    mapped_count--;
    vmaUnmapMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
}

VulkanImage::VulkanImage(VulkanDevice& device, TextureDesc desc, std::span<const std::byte> initial_data) : Texture(device, std::move(desc)) {
    const auto logger = device.GetLogger();

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

        image        = vk::raii::Image(device.GetDevice(), to_vk_image_create_info(m_Desc), device.GetCustomAllocator());
        image_handle = **image;
        create_vk_debug_object_info(image.value(), GetName(), device.GetDevice());
    }

    logger->trace("Allocate memory for Texture({})...", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
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
            auto error_message = fmt::format("failed to allocate memory for texture({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        vmaBindImageMemory(device.GetVmaAllocator(), allocation, **image);
    }

    logger->trace("Create TextureView({})...", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
    {
        image_view = vk::raii::ImageView(device.GetDevice(), to_vk_image_view_create_info(m_Desc, **image), device.GetCustomAllocator());
        create_vk_debug_object_info(image_view.value(), GetName(), device.GetDevice());
    }

    if (!initial_data.empty()) {
        logger->trace("Copy initial data to texture({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
        if (utils::has_flag(m_Desc.usages, TextureUsageFlags::CopyDst)) {
            VulkanBuffer staging_buffer(
                device, GPUBufferDesc{
                            .name         = std::pmr::string(fmt::format("{}_staging", GetName())),
                            .element_size = initial_data.size_bytes(),
                            .usages       = GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::MapWrite,
                        },
                initial_data);

            auto  context    = device.CreateCopyContext("StageBufferToTexture");
            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);

            context->Begin();
            context->ResourceBarrier(
                {}, {},
                {{
                    Transition(BarrierAccess::CopyDst, TextureLayout::CopyDst, PipelineStage::Copy),
                }});

            context->CopyBufferToTexture(
                staging_buffer,
                0,
                *this,
                {0, 0, 0},
                {m_Desc.width, m_Desc.height, m_Desc.depth},
                {0, 0, m_Desc.array_size});

            context->End();
            copy_queue.Submit({{*context}});

            // improve me
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "the texture({}) can not initialize with staging buffer without {}, the actual flags are {}",
                fmt::styled(GetName(), fmt::fg(fmt::color::red)),
                fmt::styled(TextureUsageFlags::CopyDst, fmt::fg(fmt::color::green)),
                fmt::styled(desc.usages, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
}

VulkanImage::VulkanImage(const VulkanSwapChain& _swap_chian, std::uint32_t index)
    : Texture(_swap_chian.GetDevice(),
              {
                  .name        = std::pmr::string(fmt::format("{}-texture-{}", _swap_chian.GetName(), index)),
                  .width       = _swap_chian.GetWidth(),
                  .height      = _swap_chian.GetHeight(),
                  .format      = _swap_chian.GetFormat(),
                  .clear_value = _swap_chian.GetDesc().clear_color,
                  .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopyDst,
              }),
      swap_chain(&_swap_chian) {
    const auto& vk_device = static_cast<VulkanDevice&>(m_Device);
    const auto  _images   = _swap_chian.GetVkSwapChain().getImages();
    create_vk_debug_object_info(_images.at(index), GetName(), vk_device.GetDevice());

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

    create_vk_debug_object_info(image_view.value(), GetName(), vk_device.GetDevice());
}

VulkanImage::~VulkanImage() {
    if (allocation) vmaFreeMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
}

VulkanSampler::VulkanSampler(VulkanDevice& device, SamplerDesc desc) : Sampler(device, std::move(desc)) {
    sampler = std::make_unique<vk::raii::Sampler>(device.GetDevice(), to_vk_sampler_create_info(m_Desc), device.GetCustomAllocator());
    create_vk_debug_object_info(*sampler, GetName(), device.GetDevice());
}

VulkanSwapChain::SemaphorePair::SemaphorePair(VulkanDevice& device, std::string_view name)
    : image_available(std::make_shared<vk::raii::Semaphore>(device.GetDevice(), vk::SemaphoreCreateInfo{}, device.GetCustomAllocator())),
      presentable(std::make_shared<vk::raii::Semaphore>(device.GetDevice(), vk::SemaphoreCreateInfo{}, device.GetCustomAllocator())) {
    create_vk_debug_object_info(*image_available, fmt::format("{}-image-available", name), device.GetDevice());
    create_vk_debug_object_info(*presentable, fmt::format("{}-presentable", name), device.GetDevice());
}

VulkanSwapChain::VulkanSwapChain(VulkanDevice& device, SwapChainDesc desc)
    : SwapChain(device, std::move(desc)), m_SemaphorePair(device, GetName())

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

auto VulkanSwapChain::AcquireTextureForRendering() -> Texture& {
    if (m_CurrentIndex != -1) {
        return *m_Images[m_CurrentIndex];
    }

    auto [result, index] = m_SwapChain->acquireNextImage(
        std::numeric_limits<std::uint64_t>::max(),
        **m_SemaphorePair.image_available);

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire next image");
    }

    m_CurrentIndex = index;

    return *m_Images[m_CurrentIndex];
}

void VulkanSwapChain::Present() {
    // window is not valid
    switch (m_Desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            if (!IsWindow(static_cast<HWND>(m_Desc.window.ptr))) return;
        } break;
#endif
        case utils::Window::Type::SDL2: {
            if (!(SDL_GetWindowFlags(static_cast<SDL_Window*>(m_Desc.window.ptr)) & SDL_WINDOW_SHOWN)) return;
        } break;
    }

    if (m_CurrentIndex == -1) return;

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
        throw std::runtime_error(fmt::format(
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
        .imageExtent      = vk::Extent2D{m_Size.x, m_Size.y},
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        .preTransform     = preTransform,
        .compositeAlpha   = composite_alpha,
        .presentMode      = vk::PresentModeKHR::eFifo,
        .clipped          = true,
    };

    // We use graphics queue as present queue, so we do not care the ownership of images
    m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(vk_device.GetDevice(), swapchain_create_info, vk_device.GetCustomAllocator());

    create_vk_debug_object_info(*m_SwapChain, GetName(), vk_device.GetDevice());
}

void VulkanSwapChain::CreateImageViews() {
    m_Images.clear();
    for (std::uint32_t index = 0; index < m_NumImages; index++) {
        m_Images.emplace_back(std::make_unique<VulkanImage>(*this, index));
    }
}

VulkanShader::VulkanShader(VulkanDevice& device, ShaderDesc desc)
    : Shader(device, std::move(desc)),
      binary_program(device.GetShaderCompiler().CompileToSPIRV(m_Desc)),
      shader(
          device.GetDevice(),
          {
              .codeSize = binary_program.GetDataSize(),
              .pCode    = reinterpret_cast<const std::uint32_t*>(binary_program.GetData()),
          },
          device.GetCustomAllocator())

{
    create_vk_debug_object_info(shader, GetName(), device.GetDevice());
}

auto VulkanShader::GetSPIRVData() const noexcept -> std::span<const std::byte> {
    return binary_program.Span<const std::byte>();
}

VulkanRenderPipeline::VulkanRenderPipeline(VulkanDevice& device, RenderPipelineDesc desc) : RenderPipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();

    bool has_vertex_shader = false, has_fragment_shader = false;
    for (const auto& p_shader : m_Desc.shaders) {
        if (auto shader = p_shader.lock(); shader) {
            if (shader->GetDesc().type == ShaderType::Vertex) {
                has_vertex_shader = true;
            } else if (shader->GetDesc().type == ShaderType::Pixel) {
                has_fragment_shader = true;
            }
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
            m_Desc.shaders.begin(), m_Desc.shaders.end(),
            [](const auto& _lhs, const auto& _rhs) {
                auto lhs = _lhs.lock();
                auto rhs = _rhs.lock();
                return lhs && rhs && lhs->GetDesc().type == rhs->GetDesc().type;
            });
        iter != m_Desc.shaders.end()) {
        const auto error_message = fmt::format(
            "shader({}) and shader({}) are same type for pipeline({})",
            fmt::styled(iter->lock()->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled((++iter)->lock()->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    // verify all shader are vulkan shader
    if (auto iter = std::find_if(
            m_Desc.shaders.begin(), m_Desc.shaders.end(),
            [](const auto& _shader) {
                auto shader = _shader.lock();
                return shader && !std::dynamic_pointer_cast<VulkanShader>(shader);
            });
        iter != m_Desc.shaders.end()) {
        const auto error_message = fmt::format(
            "shader({}) is not vulkan shader for pipeline({})",
            fmt::styled(iter->lock()->GetName(), fmt::fg(fmt::color::red)),
            fmt::styled(GetName(), fmt::fg(fmt::color::red)));
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

    auto vertex_input_attribute_descriptions = m_Desc.vertex_input_layout |
                                               ranges::views::enumerate |
                                               ranges::views::transform([](const auto& pair) {
                                                   const auto& [location, attribute] = pair;
                                                   return vk::VertexInputAttributeDescription{
                                                       .location = static_cast<std::uint32_t>(location),
                                                       .binding  = attribute.binding,
                                                       .format   = to_vk_format(attribute.format),
                                                       .offset   = static_cast<std::uint32_t>(attribute.offset),
                                                   };
                                               }) |
                                               ranges::to<std::pmr::vector<vk::VertexInputAttributeDescription>>();

    // The sort all semantic strings according to declaration and assign Location numbers sequentially to the corresponding SPIR-V variables.
    // https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#implicit-location-number-assignment
    auto vertex_input_binding_descriptions = m_Desc.vertex_input_layout |
                                             ranges::views::transform([](const auto& attribute) {
                                                 return vk::VertexInputBindingDescription{
                                                     .binding   = attribute.binding,
                                                     .stride    = static_cast<std::uint32_t>(attribute.stride),
                                                     .inputRate = attribute.per_instance
                                                                      ? vk::VertexInputRate::eInstance
                                                                      : vk::VertexInputRate::eVertex,
                                                 };
                                             }) |
                                             ranges::views::unique([](const auto& lhs, const auto& rhs) {
                                                 if (lhs.binding == rhs.binding && (lhs.stride != rhs.stride || lhs.inputRate != rhs.inputRate))
                                                     throw std::invalid_argument("Vertex input binding description is not same");
                                                 return lhs.binding == rhs.binding;
                                             }) |
                                             ranges::to<std::pmr::vector<vk::VertexInputBindingDescription>>();

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

    const auto& pipeline_layout = *static_cast<VulkanBindlessUtils&>(device.GetBindlessUtils()).pipeline_layout;

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
                .layout              = *pipeline_layout,
                .renderPass          = nullptr,
            },
            vk::PipelineRenderingCreateInfo{
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &render_format,
                .depthAttachmentFormat   = depth_format,
                .stencilAttachmentFormat = stencil_format,
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
        create_vk_debug_object_info(*pipeline, GetName(), device.GetDevice());
    }
}

VulkanComputePipeline::VulkanComputePipeline(VulkanDevice& device, ComputePipelineDesc desc) : ComputePipeline(device, std::move(desc)) {
    const auto logger = device.GetLogger();

    auto compute_shader = std::dynamic_pointer_cast<VulkanShader>(m_Desc.cs.lock());

    if (compute_shader == nullptr) {
        const auto error_message = fmt::format("Compute shader is not specified for pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    const vk::PipelineShaderStageCreateInfo shader_stage_create_info{
        .stage  = vk::ShaderStageFlagBits::eCompute,
        .module = *compute_shader->shader,
        .pName  = compute_shader->GetDesc().entry.data(),
    };

    const auto& pipeline_layout = *static_cast<VulkanBindlessUtils&>(device.GetBindlessUtils()).pipeline_layout;

    logger->trace("Create compute pipeline({})", fmt::styled(GetName(), fmt::fg(fmt::color::green)));
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
        create_vk_debug_object_info(*pipeline, GetName(), device.GetDevice());
    }
}

}  // namespace hitagi::gfx