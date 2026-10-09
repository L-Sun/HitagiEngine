export module gfx.vulkan:configs;
import interop.vulkan;
import std;
import utils;
import math;
import core;
import gfx.base;

export namespace hitagi::gfx {

constexpr std::array<const char*, 0> required_instance_layers = {
#ifdef HITAGI_DEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
};

constexpr std::array required_instance_extensions = {
    vk::KHRSurfaceExtensionName,
#ifdef HITAGI_DEBUG
    vk::EXTDebugUtilsExtensionName,
#endif
#if defined(_WIN32)
    vk::KHRWin32SurfaceExtensionName,
#elif defined(__linux__)
    vk::KHRWaylandSurfaceExtensionName,
#endif
};

constexpr std::array required_device_extensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRDynamicRenderingExtensionName,
    vk::EXTDescriptorHeapExtensionName,
    vk::KHRShaderUntypedPointersExtensionName,
    vk::EXTHostImageCopyExtensionName,
};

constexpr auto max_storage_descriptors       = 1'0000u;
constexpr auto max_sampled_image_descriptors = 1'0000u;
constexpr auto max_storage_image_descriptors = 1'0000u;
constexpr auto max_sampler_descriptors       = 128u;

}  // namespace hitagi::gfx
