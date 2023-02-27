#pragma once
#include <vulkan/vulkan_raii.hpp>

#include <array>

namespace hitagi::gfx {

constexpr std::array required_instance_layers = {
#ifdef HITAGI_DEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
};

constexpr std::array required_instance_extensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
};

constexpr std::array required_physical_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

}  // namespace hitagi::gfx