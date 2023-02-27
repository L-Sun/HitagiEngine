#include "utils.hpp"
#include "vk_device.hpp"

#undef near
#undef far
#include <hitagi/math/transform.hpp>

#include <fmt/color.h>
#include <spdlog/logger.h>

namespace hitagi::gfx {
auto custom_vk_allocation_fn(void* p_this, std::size_t size, std::size_t alignment, VkSystemAllocationScope) -> void* {
    auto& allocation_record = static_cast<VulkanDevice*>(p_this)->GetCustomAllocationRecord();

    auto allocator = std::pmr::get_default_resource();
    auto ptr       = allocator->allocate(size, alignment);
    allocation_record.emplace(ptr, std::make_pair(size, alignment));
    return ptr;
}

auto custom_vk_reallocation_fn(void* p_this, void* orign_ptr, std::size_t new_size, std::size_t alignment, VkSystemAllocationScope) -> void* {
    auto& allocation_record = static_cast<VulkanDevice*>(p_this)->GetCustomAllocationRecord();

    auto allocator = std::pmr::get_default_resource();
    // just like allocation
    if (orign_ptr == nullptr) {
        auto new_ptr = allocator->allocate(new_size, alignment);
        allocation_record.emplace(new_ptr, std::make_pair(new_size, alignment));
        return new_ptr;
    }
    // just like free
    else if (new_size == 0) {
        auto [old_size, old_alignment] = allocation_record.at(orign_ptr);
        allocation_record.erase(orign_ptr);
        allocator->deallocate(orign_ptr, old_size);
        return nullptr;
    }
    // reallocation
    else {
        // allocate new memory
        auto new_ptr = allocator->allocate(new_size, alignment);
        allocation_record.emplace(new_ptr, std::make_pair(new_size, alignment));

        auto [old_size, old_alignment] = allocation_record.at(orign_ptr);
        allocation_record.erase(orign_ptr);

        // copy origin data to new one
        std::memcpy(new_ptr, orign_ptr, std::min(old_size, new_size));

        // free origin data
        allocator->deallocate(orign_ptr, old_size);

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
    auto logger    = static_cast<spdlog::logger*>(p_logger);
    auto serverity = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(_severity);
    auto type      = static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(_type);

    std::pmr::vector<std::pmr::string> messages;
    messages.emplace_back(fmt::format(
        "{}[{}]:{} -------------",
        fmt::styled(vk::to_string(type), fmt::fg(type == vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance ? fmt::color::orange : fmt::color::red)),
        p_data->pMessageIdName,
        p_data->messageIdNumber));

    messages.emplace_back(p_data->pMessage);

    if (p_data->queueLabelCount > 0) {
        std::pmr::string queue_labels;
        for (std::uint32_t i = 0; i < p_data->queueLabelCount; i++) {
            auto color = math::to_hex(p_data->pQueueLabels->color);
            queue_labels += fmt::format("{}, ", fmt::styled(p_data->pQueueLabels[i].pLabelName, fmt::fg(fmt::rgb(color.r, color.g, color.b))));
            // remove last ", "
            if (i == p_data->queueLabelCount - 1) {
                queue_labels.pop_back();
                queue_labels.pop_back();
            }
        }
        messages.emplace_back(fmt::format("Queue Labels: {}", queue_labels));
    }

    if (p_data->cmdBufLabelCount > 0) {
        std::pmr::string cmd_buf_labels;
        for (std::uint32_t i = 0; i < p_data->cmdBufLabelCount; i++) {
            auto color = math::to_hex(p_data->pCmdBufLabels->color);
            cmd_buf_labels += fmt::format("{}, ", fmt::styled(p_data->pCmdBufLabels[i].pLabelName, fmt::fg(fmt::rgb(color.r, color.g, color.b))));
            // remove last ", "
            if (i == p_data->cmdBufLabelCount - 1) {
                cmd_buf_labels.pop_back();
                cmd_buf_labels.pop_back();
            }
        }
        messages.emplace_back(fmt::format("Command Buffer Labels: {}", cmd_buf_labels));
    }

    if (p_data->objectCount > 0) {
        std::pmr::string objects;
        for (std::uint32_t i = 0; i < p_data->objectCount; i++) {
            objects += fmt::format(
                "{}({}, {}), ",
                p_data->pObjects->pObjectName == nullptr ? "" : p_data->pObjects->pObjectName,
                vk::to_string(static_cast<vk::ObjectType>(p_data->pObjects[i].objectType)),
                p_data->pObjects[i].objectHandle);
            // remove last ", "
            if (i == p_data->objectCount - 1) {
                objects.pop_back();
                objects.pop_back();
            }
        }
        messages.emplace_back(fmt::format("Objects: {}", objects));
    }

    messages.emplace_back(messages.front() + "End");

    for (const auto& message : messages) {
        switch (serverity) {
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
                break;
            default:
                break;
        }
    }
    return false;
}
}  // namespace hitagi::gfx