#pragma once
#include <vulkan/vulkan_raii.hpp>

#include <bitset>

namespace hitagi::gfx {
inline auto compute_physical_device_scroe(const vk::raii::PhysicalDevice& device) noexcept {
    int scroe = 0;

    const auto device_properties = device.getProperties();
    const auto device_features   = device.getFeatures();

    if (device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        scroe += 1000;
    }

    scroe += device_properties.limits.maxImageDimension2D;

    if (!device_features.geometryShader) {
        scroe = 0;
    }

    return scroe;
}

inline auto get_queue_create_infos(const vk::raii::PhysicalDevice& device, const float& priority) {
    const auto queue_family_properties = device.getQueueFamilyProperties();

    // for graphics, compute, transfer(copy) queues
    std::array<vk::DeviceQueueCreateInfo, 3> queue_create_infos;
    std::array<vk::QueueFlags, 3>            queue_types = {
        vk::QueueFlagBits::eGraphics,
        vk::QueueFlagBits::eCompute,
        vk::QueueFlagBits::eTransfer,
    };
    std::bitset<3> enabled_queues;

    for (std::uint32_t family_index = 0; family_index < queue_family_properties.size(); family_index++) {
        for (std::size_t type_index = 0; type_index < queue_create_infos.size(); type_index++) {
            if (!enabled_queues[type_index] && queue_family_properties[family_index].queueFlags & queue_types[type_index]) {
                queue_create_infos[type_index] = vk::DeviceQueueCreateInfo{
                    .queueFamilyIndex = family_index,
                    .queueCount       = 1,
                    .pQueuePriorities = &priority,
                };
                enabled_queues.set(type_index);
                break;  // each unique queue family need only create once
            }
        }
    }

    if (!enabled_queues.all()) {
        throw std::runtime_error("Failed to find a queue family that supports graphics, compute, transfer operations!");
    }

    return queue_create_infos;
}
}  // namespace hitagi::gfx