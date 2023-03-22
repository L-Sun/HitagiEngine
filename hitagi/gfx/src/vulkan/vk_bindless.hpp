#pragma once
#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanBindlessUtils {
    VulkanBindlessUtils() = default;
    VulkanBindlessUtils(VulkanDevice& device);

    std::unique_ptr<vk::raii::DescriptorPool>       pool;
    std::pmr::vector<vk::raii::DescriptorSetLayout> set_layouts;
    std::pmr::vector<vk::PushConstantRange>         push_constant_ranges;
    std::unique_ptr<vk::raii::PipelineLayout>       pipeline_layout;

    std::unique_ptr<vk::raii::DescriptorSet> set;
};
}  // namespace hitagi::gfx