#include "utils.hpp"
#include "vk_device.hpp"

#undef near
#undef far
#include <hitagi/math/transform.hpp>

#include <fmt/color.h>
#include <spdlog/logger.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace hitagi::gfx {
auto custom_vk_allocation_fn(void* p_this, std::size_t size, std::size_t alignment, VkSystemAllocationScope) -> void* {
    auto& allocation_record = static_cast<VulkanDevice*>(p_this)->GetCustomAllocationRecord();

    auto allocator = std::pmr::get_default_resource();
    auto ptr       = allocator->allocate(size, alignment);
    allocation_record.emplace(ptr, std::make_pair(size, alignment));
    return ptr;
}

auto custom_vk_reallocation_fn(void* p_this, void* origin_ptr, std::size_t new_size, std::size_t alignment, VkSystemAllocationScope) -> void* {
    auto& allocation_record = static_cast<VulkanDevice*>(p_this)->GetCustomAllocationRecord();

    auto allocator = std::pmr::get_default_resource();
    // just like allocation
    if (origin_ptr == nullptr) {
        auto new_ptr = allocator->allocate(new_size, alignment);
        allocation_record.emplace(new_ptr, std::make_pair(new_size, alignment));
        return new_ptr;
    }
    // just like free
    else if (new_size == 0) {
        auto [old_size, old_alignment] = allocation_record.at(origin_ptr);
        allocation_record.erase(origin_ptr);
        allocator->deallocate(origin_ptr, old_size);
        return nullptr;
    }
    // reallocation
    else {
        // allocate new memory
        auto new_ptr = allocator->allocate(new_size, alignment);
        allocation_record.emplace(new_ptr, std::make_pair(new_size, alignment));

        auto [old_size, old_alignment] = allocation_record.at(origin_ptr);
        allocation_record.erase(origin_ptr);

        // copy origin data to new one
        std::memcpy(new_ptr, origin_ptr, std::min(old_size, new_size));

        // free origin data
        allocator->deallocate(origin_ptr, old_size);

        return new_ptr;
    }
}

auto custom_vk_free_fn(void* p_this, void* ptr) -> void {
    if (ptr == nullptr) return;

    auto& allocation_record = static_cast<VulkanDevice*>(p_this)->GetCustomAllocationRecord();

    auto allocator         = std::pmr::get_default_resource();
    auto [size, alignment] = allocation_record.at(ptr);
    allocation_record.erase(ptr);
    allocator->deallocate(ptr, size);
}

auto custom_debug_message_fn(VkDebugUtilsMessageSeverityFlagBitsEXT _severity, VkDebugUtilsMessageTypeFlagsEXT _type, VkDebugUtilsMessengerCallbackDataEXT const* p_data, void* p_logger) -> VkBool32 {
    auto logger   = static_cast<spdlog::logger*>(p_logger);
    auto severity = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(_severity);
    auto type     = static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(_type);

    std::pmr::vector<std::pmr::string> messages;

    auto message_id = fmt::format("{:#x}", static_cast<std::uint32_t>(p_data->messageIdNumber));
    messages.emplace_back(fmt::format(
        "{}[ {} ]:{} -------------",
        fmt::styled(vk::to_string(type), fmt::fg(type == vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance ? fmt::color::orange : fmt::color::red)),
        p_data->pMessageIdName,
        message_id));

    std::string_view detail{p_data->pMessage};
    detail.remove_prefix(detail.find(message_id) + message_id.size() + 3);
    messages.emplace_back(detail);

    if (p_data->queueLabelCount > 0) {
        messages.emplace_back("Queue Labels:");
        for (std::uint32_t i = 0; i < p_data->queueLabelCount; i++) {
            auto color     = math::to_hex(p_data->pQueueLabels->color);
            auto fmt_color = fmt::rgb(color.r, color.g, color.b);
            messages.emplace_back(fmt::format("{}, ", fmt::styled(p_data->pQueueLabels[i].pLabelName, fmt::fg(fmt_color))));
        }
    }

    if (p_data->cmdBufLabelCount > 0) {
        messages.emplace_back("Command Buffer Labels: ");
        for (std::uint32_t i = 0; i < p_data->cmdBufLabelCount; i++) {
            auto color     = math::to_hex(p_data->pCmdBufLabels->color);
            auto fmt_color = fmt::rgb(color.r, color.g, color.b);
            messages.emplace_back(fmt::format("{}, ", fmt::styled(p_data->pCmdBufLabels[i].pLabelName, fmt::fg(fmt_color))));
        }
    }

    if (p_data->objectCount > 0) {
        messages.emplace_back("Objects:");
        for (std::uint32_t i = 0; i < p_data->objectCount; i++) {
            messages.emplace_back(fmt::format(
                "\t {}({:>12}, {:#018x})",
                p_data->pObjects->pObjectName == nullptr ? "" : p_data->pObjects->pObjectName,
                vk::to_string(static_cast<vk::ObjectType>(p_data->pObjects[i].objectType)),
                p_data->pObjects[i].objectHandle));
        }
    }

    messages.emplace_back(messages.front() + "End");
    bool error = false;
    for (const auto& message : messages) {
        switch (severity) {
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                logger->trace(message);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                logger->info(message);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                logger->warn(message);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                logger->error(message);
                error = true;
                break;
            default:
                break;
        }
    }
    if (error) {
        throw std::runtime_error("Vulkan Error");
    }
    return false;
}

}  // namespace hitagi::gfx