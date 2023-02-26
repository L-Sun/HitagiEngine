#pragma once
#include <vulkan/vulkan_raii.hpp>

#include <array>

namespace hitagi::gfx {

constexpr std::array try_enable_instance_layers = {
#ifdef HITAGI_DEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
};

constexpr std::array try_enable_instance_extensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};

}  // namespace hitagi::gfx