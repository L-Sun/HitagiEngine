#pragma once
#include <vulkan/vulkan_raii.hpp>

#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_wayland.h>
#endif

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
#if defined(_WIN32)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
    VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
};

constexpr std::array required_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr vk::PhysicalDeviceVulkan12Features required_physical_device_features = {
    .timelineSemaphore = true,
};

}  // namespace hitagi::gfx