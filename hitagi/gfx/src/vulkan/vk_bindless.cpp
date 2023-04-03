#include "vk_bindless.hpp"
#include "vk_device.hpp"
#include "vk_resource.hpp"
#include "utils.hpp"

#include <spdlog/logger.h>
#include <fmt/color.h>

#include <numeric>

namespace hitagi::gfx {
VulkanBindlessUtils::VulkanBindlessUtils(VulkanDevice& device, std::string_view name)
    : BindlessUtils(device, name),
      bindless_handle_pools{{
          {{}, std::mutex{}},
          {{}, std::mutex{}},
          {{}, std::mutex{}},
          {{}, std::mutex{}},
      }} {
    const auto logger = device.GetLogger();

    const auto limits = device.GetPhysicalDevice().getProperties().limits;

    const std::array<vk::DescriptorPoolSize, 4> pool_sizes = {{
        {vk::DescriptorType::eStorageBuffer, limits.maxDescriptorSetStorageBuffers},
        {vk::DescriptorType::eSampledImage, limits.maxDescriptorSetSampledImages},
        {vk::DescriptorType::eStorageImage, limits.maxDescriptorSetStorageImages},
        {vk::DescriptorType::eSampler, limits.maxDescriptorSetSamplers},
        // TODO ray tracing
    }};

    const std::array descriptor_counts = {
        limits.maxDescriptorSetStorageBuffers,
        limits.maxDescriptorSetSampledImages,
        limits.maxDescriptorSetStorageImages,
        limits.maxDescriptorSetSamplers,
    };

    static_assert(pool_sizes.size() == std::tuple_size<decltype(bindless_handle_pools)>());

    const vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
                 vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
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

    bindless_info_constant_range = {
        .stageFlags = vk::ShaderStageFlagBits::eAll,
        .offset     = 0,
        .size       = sizeof(BindlessInfoOffset),
    };

    std::pmr::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts;
    std::transform(
        set_layouts.begin(), set_layouts.end(),
        std::back_inserter(vk_descriptor_set_layouts),
        [](const auto& layout) { return *layout; });

    logger->trace("Creating bindless pipeline layout...");
    {
        pipeline_layout = std::make_unique<vk::raii::PipelineLayout>(
            device.GetDevice(),
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount         = static_cast<std::uint32_t>(vk_descriptor_set_layouts.size()),
                .pSetLayouts            = vk_descriptor_set_layouts.data(),
                .pushConstantRangeCount = 1,
                .pPushConstantRanges    = &bindless_info_constant_range,
            },
            device.GetCustomAllocator());
        create_vk_debug_object_info(*pipeline_layout, fmt::format("{}-BindlessPipelineLayout", device.GetName()), device.GetDevice());
    }

    logger->trace("Allocator Descriptors");
    {
        const vk::StructureChain descriptor_set_allocate_info = {
            vk::DescriptorSetAllocateInfo{
                .descriptorPool     = **pool,
                .descriptorSetCount = static_cast<std::uint32_t>(vk_descriptor_set_layouts.size()),
                .pSetLayouts        = vk_descriptor_set_layouts.data(),
            },
            vk::DescriptorSetVariableDescriptorCountAllocateInfo{
                .descriptorSetCount = static_cast<std::uint32_t>(vk_descriptor_set_layouts.size()),
                .pDescriptorCounts  = descriptor_counts.data(),
            },
        };

        descriptor_sets = device.GetDevice().allocateDescriptorSets(descriptor_set_allocate_info.get());

        for (std::size_t set_index = 0; set_index < descriptor_sets.size(); set_index++) {
            create_vk_debug_object_info(
                descriptor_sets[set_index],
                fmt::format("{}-BindlessDescriptorSet-{}", device.GetName(), vk::to_string(pool_sizes[set_index].type)),
                device.GetDevice());
        }
    }

    logger->trace("Initial Bindless Handle Pool...");
    {
        for (std::uint8_t set_index = 0; set_index < pool_sizes.size(); set_index++) {
            const auto& pool_size = pool_sizes[set_index];

            auto& handle_pool = bindless_handle_pools.at(set_index).pool;
            handle_pool.reserve(pool_size.descriptorCount);

            for (std::uint32_t index = 0; index < pool_size.descriptorCount; index++) {
                handle_pool.emplace_back(BindlessHandle{
                    .index   = index,
                    .version = 0,
                    .type    = set_index,
                    .tag     = 0,
                });
            }
        }
    }
}

auto VulkanBindlessUtils::CreateBindlessHandle(GPUBuffer& buffer, std::size_t index, bool writable) -> BindlessHandle {
    if (writable && !utils::has_flag(buffer.GetDesc().usages, GPUBufferUsageFlags::Storage)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: buffer({}) is not writable",
            fmt::styled(buffer.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }
    // ! For now, we don't need to check this because we use storage buffer to simulate constant buffer
    // if (!writable && !utils::has_flag(buffer.GetDesc().usages, GPUBufferUsageFlags::Constant)) {
    //     const auto error_message = fmt::format(
    //         "Failed to create BindlessHandle: buffer({}) is not used for shader visible",
    //         fmt::styled(buffer.GetName(), fmt::fg(fmt::color::red)));
    //     m_Device.GetLogger()->error(error_message);
    //     throw std::invalid_argument(error_message);
    // }

    auto& vk_device = static_cast<VulkanDevice&>(m_Device);

    auto& [pool, mutex] = bindless_handle_pools[0];
    std::lock_guard lock{mutex};

    auto handle = pool.back();
    handle.tag  = writable ? 1 : 0;

    const vk::DescriptorBufferInfo buffer_info{
        .buffer = **static_cast<VulkanBuffer&>(buffer).buffer,
        .offset = index * buffer.GetDesc().element_size,
        .range  = buffer.GetDesc().element_size,
    };

    const vk::WriteDescriptorSet write_info{
        .dstSet          = *descriptor_sets[0],
        .dstBinding      = 0,
        .dstArrayElement = handle.index,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo     = &buffer_info,
    };

    vk_device.GetDevice().updateDescriptorSets(write_info, {});

    pool.pop_back();

    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(Texture& texture, bool writable) -> BindlessHandle {
    if (writable && !utils::has_flag(texture.GetDesc().usages, TextureUsageFlags::UAV)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: texture({}) is not writable",
            fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }
    if (!writable && !utils::has_flag(texture.GetDesc().usages, TextureUsageFlags::SRV)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: texture({}) is not used for shader visible",
            fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    auto& vk_device = static_cast<VulkanDevice&>(m_Device);

    auto& [pool, mutex] = bindless_handle_pools[1];
    std::lock_guard lock{mutex};

    auto handle = pool.back();
    handle.tag  = writable ? 1 : 0;

    const vk::DescriptorImageInfo image_info{
        .sampler     = nullptr,
        .imageView   = **static_cast<VulkanImage&>(texture).image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    const vk::WriteDescriptorSet write_info{
        .dstSet          = *descriptor_sets[1],
        .dstBinding      = 0,
        .dstArrayElement = handle.index,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eSampledImage,
        .pImageInfo      = &image_info,
    };

    vk_device.GetDevice().updateDescriptorSets(write_info, {});

    pool.pop_back();

    return handle;
}

void VulkanBindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    auto& [pool, mutex] = bindless_handle_pools[handle.type];
    std::lock_guard lock{mutex};
    handle.version++;
    pool.emplace_back(handle);
}

}  // namespace hitagi::gfx