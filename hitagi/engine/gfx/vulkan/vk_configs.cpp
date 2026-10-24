export module gfx.vulkan:configs;
import interop.vulkan;
import interop.sdl;
import std;
import utils;
import math;
import core;
import gfx.base;

export namespace hitagi::gfx {

constexpr std::array required_instance_extensions = {
    vk::KHRSurfaceExtensionName,
#ifdef HITAGI_DEBUG
    vk::EXTDebugUtilsExtensionName,
#endif
#if defined(_WIN32)
    vk::KHRWin32SurfaceExtensionName,
#endif
};

auto get_instance_extensions() -> std::pmr::vector<const char*>;
auto choose_surface_format(std::span<const vk::SurfaceFormatKHR> formats) -> vk::SurfaceFormatKHR;

constexpr std::array required_device_extensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRDynamicRenderingExtensionName,
    vk::KHRShaderUntypedPointersExtensionName,
    vk::EXTHostImageCopyExtensionName,
};

constexpr auto max_storage_descriptors       = 1'0000u;
constexpr auto max_sampled_image_descriptors = 1'0000u;
constexpr auto max_storage_image_descriptors = 1'0000u;
constexpr auto max_sampler_descriptors       = 128u;

// Storage buffer, sampled image, storage image, sampler; Slang's non-mutable set 0 bindings.
constexpr std::array vulkan_bindless_descriptor_counts{max_storage_descriptors, max_sampled_image_descriptors, max_storage_image_descriptors, max_sampler_descriptors};
constexpr std::array<std::uint32_t, 4> vulkan_bindless_descriptor_bindings{7, 2, 3, 0};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

auto choose_surface_format(std::span<const vk::SurfaceFormatKHR> formats) -> vk::SurfaceFormatKHR {
    // Renderers and GUI already output display-encoded colors. An sRGB attachment
    // would encode them again; match the UNORM presentation targets of DX12/Metal.
    for (const auto format : {vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm}) {
        for (const auto& supported : formats) {
            if (supported.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear &&
                (supported.format == format || (formats.size() == 1 && supported.format == vk::Format::eUndefined))) {
                return {.format = format, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear};
            }
        }
    }
    throw std::runtime_error("Surface does not support an RGBA8/BGRA8 UNORM swapchain with sRGB nonlinear presentation");
}

auto get_instance_extensions() -> std::pmr::vector<const char*> {
    std::pmr::vector<const char*> instance_extensions(required_instance_extensions.begin(), required_instance_extensions.end());
    // SDL_Vulkan_CreateSurface needs the WSI extensions of whichever video driver SDL picked at runtime.
    if (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) {
        std::uint32_t sdl_extension_count = 0;
        if (const auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count)) {
            instance_extensions.insert(instance_extensions.end(), sdl_extensions, sdl_extensions + sdl_extension_count);
        } else {
            throw std::runtime_error(std::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError()));
        }
    }
    const auto as_string_view = [](const char* name) { return std::string_view{name}; };
    std::ranges::sort(instance_extensions, {}, as_string_view);
    const auto duplicates = std::ranges::unique(instance_extensions, {}, as_string_view);
    instance_extensions.erase(duplicates.begin(), duplicates.end());
    return instance_extensions;
}

}  // namespace hitagi::gfx
