#include "vk_bindless.hpp"
#include "vk_device.hpp"
#include "utils.hpp"

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {
VulkanBindlessUtils::VulkanBindlessUtils(VulkanDevice& device) {
    auto logger = device.GetLogger();

    const auto limits = device.GetPhysicalDevice().getProperties().limits;

    const std::array<vk::DescriptorPoolSize, 5> pool_sizes = {{
        {vk::DescriptorType::eSampler, limits.maxDescriptorSetSamplers},
        {vk::DescriptorType::eSampledImage, limits.maxDescriptorSetSampledImages},
        {vk::DescriptorType::eStorageImage, limits.maxDescriptorSetStorageImages},
        {vk::DescriptorType::eUniformBuffer, limits.maxDescriptorSetUniformBuffersDynamic},
        {vk::DescriptorType::eStorageBuffer, limits.maxDescriptorSetStorageBuffersDynamic},
        // TODO ray tracing
    }};

    const vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
        .flags         = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
        .maxSets       = pool_sizes.size(),  // we will make sure each set only contains one descriptor type
        .poolSizeCount = pool_sizes.size(),
        .pPoolSizes    = pool_sizes.data(),
    };

    logger->trace("Creating bindless descriptor pool...");
    pool = std::make_unique<vk::raii::DescriptorPool>(device.GetDevice(), descriptor_pool_create_info, device.GetCustomAllocator());
    create_vk_debug_object_info(*pool, fmt::format("{}-BindlessDescriptorPool", device.GetName()), device.GetDevice());

    logger->trace("Creating bindless descriptor set layout...");
    std::transform(
        pool_sizes.begin(), pool_sizes.end(),
        std::back_inserter(set_layouts),
        [&](const auto& pool_size) {
            const vk::DescriptorSetLayoutBinding binding{
                .binding            = 0,
                .descriptorType     = pool_size.type,
                .descriptorCount    = pool_size.descriptorCount,
                .stageFlags         = vk::ShaderStageFlagBits::eAll,
                .pImmutableSamplers = nullptr,
            };

            constexpr auto ext_flags = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                                       vk::DescriptorBindingFlagBits::ePartiallyBound |
                                       vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

            const vk::StructureChain descriptor_set_layout_create_info{
                vk::DescriptorSetLayoutCreateInfo{
                    .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
                    .bindingCount = 1,
                    .pBindings    = &binding,
                },
                vk::DescriptorSetLayoutBindingFlagsCreateInfo{
                    .bindingCount  = 1,
                    .pBindingFlags = &ext_flags,
                },
            };
            auto layout = vk::raii::DescriptorSetLayout(device.GetDevice(), descriptor_set_layout_create_info.get(), device.GetCustomAllocator());
            create_vk_debug_object_info(layout, fmt::format("{}-BindlessSetLayout-{}", device.GetName(), vk::to_string(pool_size.type)), device.GetDevice());
            return layout;
        });

    push_constant_ranges.emplace_back(vk::PushConstantRange{
        .stageFlags = vk::ShaderStageFlagBits::eAll,
        .offset     = 0,
        .size       = 4 * sizeof(std::uint32_t),
    });

    std::pmr::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts;
    std::transform(
        set_layouts.begin(), set_layouts.end(),
        std::back_inserter(vk_descriptor_set_layouts),
        [](const auto& layout) { return *layout; });

    logger->trace("Creating bindless pipeline layout...");
    pipeline_layout = std::make_unique<vk::raii::PipelineLayout>(
        device.GetDevice(),
        vk::PipelineLayoutCreateInfo{
            .setLayoutCount         = static_cast<std::uint32_t>(vk_descriptor_set_layouts.size()),
            .pSetLayouts            = vk_descriptor_set_layouts.data(),
            .pushConstantRangeCount = static_cast<std::uint32_t>(push_constant_ranges.size()),
            .pPushConstantRanges    = push_constant_ranges.data(),
        },
        device.GetCustomAllocator());
}
}  // namespace hitagi::gfx