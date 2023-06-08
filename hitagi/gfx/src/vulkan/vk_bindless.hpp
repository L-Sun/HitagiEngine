#pragma once
#include <hitagi/gfx/bindless.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <deque>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanBindlessUtils : public BindlessUtils {
    VulkanBindlessUtils(VulkanDevice& device, std::string_view name);

    auto CreateBindlessHandle(GPUBuffer& buffer, std::uint64_t index, bool writeable = false) -> BindlessHandle final;
    auto CreateBindlessHandle(Texture& texture, bool writeable = false) -> BindlessHandle final;
    auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle final;
    void DiscardBindlessHandle(BindlessHandle handle) final;

    std::unique_ptr<vk::raii::DescriptorPool>       pool;
    std::pmr::vector<vk::raii::DescriptorSetLayout> set_layouts;
    vk::PushConstantRange                           bindless_info_constant_range;
    std::unique_ptr<vk::raii::PipelineLayout>       pipeline_layout;

    std::vector<vk::raii::DescriptorSet> descriptor_sets;

private:
    struct BindlessHandlePool {
        std::pmr::deque<BindlessHandle> pool;
        std::mutex                      mutex{};
    };
    std::array<BindlessHandlePool, 4> m_BindlessHandlePools{};
};
}  // namespace hitagi::gfx