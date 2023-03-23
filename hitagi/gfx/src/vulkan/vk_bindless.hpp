#pragma once
#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanBindlessUtils {
    VulkanBindlessUtils() = default;
    VulkanBindlessUtils(VulkanDevice& device);

    std::unique_ptr<vk::raii::DescriptorPool>       pool;
    std::pmr::vector<vk::raii::DescriptorSetLayout> set_layouts;
    vk::PushConstantRange                           bindless_info_constant_range;
    std::unique_ptr<vk::raii::PipelineLayout>       pipeline_layout;

    std::pmr::vector<vk::raii::DescriptorSet> descriptor_sets;
};
}  // namespace hitagi::gfx