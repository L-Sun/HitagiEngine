#pragma once
#include "vk_configs.hpp"
#include "vk_sync.hpp"

#include <hitagi/gfx/command_context.hpp>
#include <hitagi/utils/array.hpp>
#include <hitagi/utils/soa.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <SDL2/SDL_vulkan.h>

#include <set>

namespace hitagi::gfx {

auto custom_vk_allocation_fn(void* p_this, std::size_t size, std::size_t alignment, VkSystemAllocationScope) -> void*;
auto custom_vk_reallocation_fn(void* p_this, void* origin_ptr, std::size_t new_size, std::size_t alignment, VkSystemAllocationScope) -> void*;
auto custom_vk_free_fn(void* p_this, void* ptr) -> void;
auto custom_debug_message_fn(VkDebugUtilsMessageSeverityFlagBitsEXT _severity, VkDebugUtilsMessageTypeFlagsEXT _type, VkDebugUtilsMessengerCallbackDataEXT const* p_data, void* p_logger) -> VkBool32;

inline auto compute_physical_device_score(const vk::raii::PhysicalDevice& device) noexcept {
    int score = 0;

    const auto device_properties = device.getProperties();
    const auto device_features   = device.getFeatures();

    if (device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    score += device_properties.limits.maxImageDimension2D;

    if (!device_features.geometryShader) {
        score = 0;
    }

    return score;
}

inline auto is_physical_suitable(const vk::raii::PhysicalDevice& physical_device) -> bool {
    const auto queue_family_properties = physical_device.getQueueFamilyProperties();

    bool queue_family_found = false;
    for (const auto& queue_family_properties : queue_family_properties) {
        if (queue_family_properties.queueCount >= magic_enum::enum_count<CommandType>() &&
            (queue_family_properties.queueFlags & vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties.queueFlags & vk::QueueFlagBits::eCompute) &&
            (queue_family_properties.queueFlags & vk::QueueFlagBits::eTransfer)) {
            queue_family_found = true;
            break;
        }
    }

    const auto                      supported_extensions = physical_device.enumerateDeviceExtensionProperties();
    std::pmr::set<std::pmr::string> required_extensions  = {required_device_extensions.begin(), required_device_extensions.end()};

    for (const auto& extension : supported_extensions) {
        required_extensions.erase(std::pmr::string{extension.extensionName});
    }
    return queue_family_found && required_extensions.empty();
}

inline auto get_queue_create_info(const vk::raii::PhysicalDevice& device) -> std::optional<vk::DeviceQueueCreateInfo> {
    const auto queue_family_properties = device.getQueueFamilyProperties();

    constexpr static auto priorities = utils::create_enum_array<float, CommandType>(1.0f);

    for (std::uint32_t family_index = 0; family_index < queue_family_properties.size(); family_index++) {
        if (queue_family_properties[family_index].queueCount >= magic_enum::enum_count<CommandType>() &&
            (queue_family_properties[family_index].queueFlags & vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties[family_index].queueFlags & vk::QueueFlagBits::eCompute) &&
            (queue_family_properties[family_index].queueFlags & vk::QueueFlagBits::eTransfer)) {
            return vk::DeviceQueueCreateInfo{
                .queueFamilyIndex = family_index,
                .queueCount       = magic_enum::enum_count<CommandType>(),
                .pQueuePriorities = priorities.data(),
            };
        };
    }
    return std::nullopt;
}

inline constexpr auto to_vk_format(Format format) noexcept -> vk::Format {
    switch (format) {
        case Format::UNKNOWN:
            return vk::Format::eUndefined;
        case Format::R32G32B32A32_FLOAT:
            return vk::Format::eR32G32B32A32Sfloat;
        case Format::R32G32B32A32_UINT:
            return vk::Format::eR32G32B32A32Uint;
        case Format::R32G32B32A32_SINT:
            return vk::Format::eR32G32B32A32Sint;
        case Format::R32G32B32_FLOAT:
            return vk::Format::eR32G32B32Sfloat;
        case Format::R32G32B32_UINT:
            return vk::Format::eR32G32B32Uint;
        case Format::R32G32B32_SINT:
            return vk::Format::eR32G32B32Sint;
        case Format::R16G16B16A16_FLOAT:
            return vk::Format::eR16G16B16A16Sfloat;
        case Format::R16G16B16A16_UNORM:
            return vk::Format::eR16G16B16A16Unorm;
        case Format::R16G16B16A16_UINT:
            return vk::Format::eR16G16B16A16Uint;
        case Format::R16G16B16A16_SNORM:
            return vk::Format::eR16G16B16A16Snorm;
        case Format::R16G16B16A16_SINT:
            return vk::Format::eR16G16B16A16Sint;
        case Format::R32G32_FLOAT:
            return vk::Format::eR32G32Sfloat;
        case Format::R32G32_UINT:
            return vk::Format::eR32G32Uint;
        case Format::R32G32_SINT:
            return vk::Format::eR32G32Sint;
        case Format::D32_FLOAT_S8X24_UINT:
            return vk::Format::eD32SfloatS8Uint;
        case Format::R11G11B10_FLOAT:
            return vk::Format::eB10G11R11UfloatPack32;
        case Format::R8G8B8A8_UNORM:
            return vk::Format::eR8G8B8A8Unorm;
        case Format::R8G8B8A8_UNORM_SRGB:
            return vk::Format::eR8G8B8A8Srgb;
        case Format::R8G8B8A8_UINT:
            return vk::Format::eR8G8B8A8Uint;
        case Format::R8G8B8A8_SNORM:
            return vk::Format::eR8G8B8A8Snorm;
        case Format::R8G8B8A8_SINT:
            return vk::Format::eR8G8B8A8Sint;
        case Format::R16G16_FLOAT:
            return vk::Format::eR16G16Sfloat;
        case Format::R16G16_UNORM:
            return vk::Format::eR16G16Unorm;
        case Format::R16G16_UINT:
            return vk::Format::eR16G16Uint;
        case Format::R16G16_SNORM:
            return vk::Format::eR16G16Snorm;
        case Format::R16G16_SINT:
            return vk::Format::eR16G16Sint;
        case Format::D32_FLOAT:
            return vk::Format::eD32Sfloat;
        case Format::R32_FLOAT:
            return vk::Format::eR32Sfloat;
        case Format::R32_UINT:
            return vk::Format::eR32Uint;
        case Format::R32_SINT:
            return vk::Format::eR32Sint;
        case Format::D24_UNORM_S8_UINT:
            return vk::Format::eD24UnormS8Uint;
        case Format::R8G8_UNORM:
            return vk::Format::eR8G8Unorm;
        case Format::R8G8_UINT:
            return vk::Format::eR8G8Uint;
        case Format::R8G8_SNORM:
            return vk::Format::eR8G8Snorm;
        case Format::R8G8_SINT:
            return vk::Format::eR8G8Sint;
        case Format::R16_FLOAT:
            return vk::Format::eR16Sfloat;
        case Format::D16_UNORM:
            return vk::Format::eD16Unorm;
        case Format::R16_UNORM:
            return vk::Format::eR16Unorm;
        case Format::R16_UINT:
            return vk::Format::eR16Uint;
        case Format::R16_SNORM:
            return vk::Format::eR16Snorm;
        case Format::R16_SINT:
            return vk::Format::eR16Sint;
        case Format::R8_UNORM:
            return vk::Format::eR8Unorm;
        case Format::R8_UINT:
            return vk::Format::eR8Uint;
        case Format::R8_SNORM:
            return vk::Format::eR8Snorm;
        case Format::R8_SINT:
            return vk::Format::eR8Sint;
        case Format::B5G6R5_UNORM:
            return vk::Format::eB5G6R5UnormPack16;
        case Format::B5G5R5A1_UNORM:
            return vk::Format::eB5G5R5A1UnormPack16;
        case Format::B8G8R8A8_UNORM:
            return vk::Format::eB8G8R8A8Unorm;
        case Format::B8G8R8X8_UNORM:
            return vk::Format::eB8G8R8A8Unorm;
        case Format::B8G8R8A8_UNORM_SRGB:
            return vk::Format::eB8G8R8A8Srgb;
        case Format::B8G8R8X8_UNORM_SRGB:
            return vk::Format::eB8G8R8A8Srgb;
        case Format::BC6H_UF16:
            return vk::Format::eBc6HUfloatBlock;
        case Format::BC6H_SF16:
            return vk::Format::eBc6HSfloatBlock;
        case Format::BC7_UNORM:
            return vk::Format::eBc7UnormBlock;
        case Format::BC7_UNORM_SRGB:
            return vk::Format::eBc7SrgbBlock;
        default:
            return vk::Format::eUndefined;
    }
}

inline auto get_sdl2_drawable_size(SDL_Window* window) -> math::vec2u {
    int width, height;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    return {width, height};
}

inline constexpr auto get_command_label_color(CommandType type) {
    constexpr std::array graphics_color = {0.48f, 0.76f, 0.47f, 1.0f};
    constexpr std::array compute_color  = {0.38f, 0.69f, 0.94f, 1.0f};
    constexpr std::array copy_color     = {0.88f, 0.39f, 0.35f, 1.0f};

    switch (type) {
        case CommandType::Graphics:
            return graphics_color;
        case CommandType::Compute:
            return compute_color;
        case CommandType::Copy:
            return copy_color;
    };
}

template <typename T>
inline constexpr auto get_vk_handle(const T& handle) {
    return reinterpret_cast<std::uintptr_t>(static_cast<typename T::CType>(*handle));
}

inline auto convert_to_sao_vk_semaphore_wait_pairs(const std::pmr::vector<SemaphoreWaitPair>& pairs) {
    utils::SoA<vk::Semaphore, std::uint64_t> result;
    std::transform(pairs.begin(), pairs.end(), std::back_inserter(result), [](const SemaphoreWaitPair& pair) {
        auto&& [semaphore, value] = pair;
        auto& vk_semaphore        = std::static_pointer_cast<VulkanSemaphore>(semaphore)->semaphore;
        return std::make_pair(*vk_semaphore, value);
    });

    return result;
}

inline constexpr auto get_shader_model_version(Shader::Type type) noexcept {
    switch (type) {
        case Shader::Type::Vertex:
            return L"vs_6_7";
        case Shader::Type::Pixel:
            return L"ps_6_7";
        case Shader::Type::Geometry:
            return L"gs_6_7";
        case Shader::Type::Compute:
            return L"cs_6_7";
    }
}

}  // namespace hitagi::gfx