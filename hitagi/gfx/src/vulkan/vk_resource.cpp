#include "vk_resource.hpp"
#include "vk_device.hpp"
#include "utils.hpp"

namespace hitagi::gfx {

VulkanSwapChain::VulkanSwapChain(VulkanDevice& device, SwapChain::Desc desc) : SwapChain(device, desc) {
    auto window_size = math::vec2u{};

#ifdef _WIN32
    vk::Win32SurfaceCreateInfoKHR surface_create_info{
        .hinstance = GetModuleHandle(nullptr),
        .hwnd      = static_cast<HWND>(desc.window_ptr),
    };
    // get window draw area size using window handle
    {
        RECT rect;
        GetClientRect(static_cast<HWND>(desc.window_ptr), &rect);
        window_size.x = rect.right - rect.left;
        window_size.y = rect.bottom - rect.top;
    }
#endif

    surface = std::make_unique<vk::raii::SurfaceKHR>(
        device.GetInstance(),
        surface_create_info,
        device.GetCustomAllocator());

    // we use graphics queue as present queue
    const auto& graphics_queue = static_cast<VulkanCommandQueue&>(device.GetCommandQueue(CommandType::Graphics));
    const auto& physcal_device = device.GetPhysicalDevice();
    if (!physcal_device.getSurfaceSupportKHR(graphics_queue.GetFramilyIndex(), **surface)) {
        throw std::runtime_error(fmt::format(
            "The graphics queue({}) of physical device({}) can not support surface(window_ptr: {})",
            graphics_queue.GetFramilyIndex(),
            physcal_device.getProperties().deviceName,
            desc.window_ptr));
    }

    const auto surface_capabilities    = physcal_device.getSurfaceCapabilitiesKHR(**surface);
    const auto supported_formats       = physcal_device.getSurfaceFormatsKHR(**surface);
    const auto supported_present_modes = physcal_device.getSurfacePresentModesKHR(**surface);

    if (std::find_if(supported_formats.begin(), supported_formats.end(), [desc](const auto& surface_format) {
            return surface_format.format == to_vk_format(desc.format);
        }) == supported_formats.end()) {
        throw std::runtime_error(fmt::format(
            "The physical device({}) can not support surface(window_ptr: {}) with format: {}",
            physcal_device.getProperties().deviceName,
            desc.window_ptr,
            magic_enum::enum_name(desc.format)));
    }

    if (std::find_if(supported_present_modes.begin(), supported_present_modes.end(), [](const auto& present_mode) {
            return present_mode == vk::PresentModeKHR::eFifo;
        }) == supported_present_modes.end()) {
        throw std::runtime_error(fmt::format(
            "The physical device({}) can not support surface(window_ptr: {}) with present mode: VK_PRESENT_MODE_FIFO_KHR",
            physcal_device.getProperties().deviceName,
            desc.window_ptr));
    }

    if (surface_capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max()) {
        // surface size is undefined, so we define it
        size.x = std::clamp(window_size.x, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        size.y = std::clamp(window_size.y, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    } else {
        // surface size is defined, so we use it
        size.x = surface_capabilities.currentExtent.width;
        size.y = surface_capabilities.currentExtent.height;
    }

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
        .surface          = **surface,
        .minImageCount    = surface_capabilities.minImageCount,
        .imageFormat      = to_vk_format(desc.format),
        .imageExtent      = vk::Extent2D{size.x, size.y},
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform     = preTransform,
        .compositeAlpha   = composite_alpha,
        .presentMode      = vk::PresentModeKHR::eFifo,
        .clipped          = true,
    };

    // We use graphics queue as present queue, so we do not care the owership of images
    swapchain = std::make_unique<vk::raii::SwapchainKHR>(device.GetDevice(), swapchain_create_info, device.GetCustomAllocator());
}

auto VulkanSwapChain::GetCurrentBackBuffer() -> Texture& {
    return GetBuffers()[0];
}
auto VulkanSwapChain::GetBuffers() -> std::pmr::vector<std::reference_wrapper<Texture>> {
    if (images.empty()) {
        auto& vk_device = static_cast<VulkanDevice&>(device);
        auto  _images   = swapchain->getImages();

        vk::ImageViewCreateInfo image_view_create_info{
            .viewType         = vk::ImageViewType::e2D,
            .format           = to_vk_format(desc.format),
            .subresourceRange = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };

        Texture::Desc texture_desc{
            .width  = size.x,
            .height = size.y,
            .format = desc.format,
            .usages = Texture::UsageFlags::RTV,
        };

        for (std::size_t i = 0; i < _images.size(); i++) {
            image_names.emplace_back(fmt::format("{}-image-{}", desc.name, i));
            texture_desc.name = image_names.back();

            images.emplace_back(std::make_shared<VulkanImage>(device, texture_desc));

            image_view_create_info.image = _images[i];
            images.back()->image_view    = vk::raii::ImageView(vk_device.GetDevice(), image_view_create_info, vk_device.GetCustomAllocator());
        }
    }

    std::pmr::vector<std::reference_wrapper<Texture>> result;
    std::transform(images.begin(), images.end(), std::back_inserter(result), [](const auto& image) { return std::ref(*image); });

    return result;
}
auto VulkanSwapChain::Width() -> std::uint32_t {
    return size.x;
}
auto VulkanSwapChain::Height() -> std::uint32_t {
    return size.y;
}
void VulkanSwapChain::Present() {
}
void VulkanSwapChain::Resize() {
}

}  // namespace hitagi::gfx