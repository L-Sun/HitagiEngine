#include "vk_command_queue.hpp"
#include "vk_device.hpp"
#include "vk_command_buffer.hpp"
#include "vk_sync.hpp"
#include "utils.hpp"

#include <fmt/color.h>
#include <spdlog/logger.h>

#include <algorithm>

namespace hitagi::gfx {

VulkanCommandQueue::VulkanCommandQueue(VulkanDevice& device, CommandType type, std::string_view name, std::uint32_t queue_family_index)
    : CommandQueue(device, type, name),
      m_FamilyIndex(queue_family_index),
      m_Queue(device.GetDevice().getQueue(queue_family_index, magic_enum::enum_integer(type))),
      m_Semaphore(std::make_shared<VulkanSemaphore>(device, fmt::format("{}-Semaphore", m_Name)))

{
    device.GetDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
        .objectType   = vk::ObjectType::eQueue,
        .objectHandle = get_vk_handle(m_Queue),
        .pObjectName  = m_Name.c_str(),
    });
}

auto VulkanCommandQueue::Submit(std::pmr::vector<CommandContext*> contexts, std::pmr::vector<SemaphoreWaitPair> wait_semaphores) -> SemaphoreWaitPair {
    // make sure all context are same command type
    if (auto iter = std::find_if(
            contexts.begin(), contexts.end(),
            [this](auto ctx) { return ctx->GetType() != m_Type; });
        iter != contexts.end()) {
        m_Device.GetLogger()->error(
            "CommandContext type({}) mismatch({}). ignore the batch contexts!!",
            fmt::styled(magic_enum::enum_name(m_Type), fmt::fg(fmt::color::red)),
            fmt::styled(magic_enum::enum_name((*iter)->GetType()), fmt::fg(fmt::color::green)));

        return {m_Semaphore, m_SubmitCount};
    }

    std::pmr::vector<vk::CommandBuffer> command_buffers;
    std::transform(
        contexts.begin(), contexts.end(),
        std::back_inserter(command_buffers),
        [](auto ctx) -> vk::CommandBuffer {
            switch (ctx->GetType()) {
                case CommandType::Graphics:
                    // return &static_cast<VulkanGraphicsCommandBuffer*>(ctx);
                case CommandType::Compute:
                    // return &static_cast<VulkanComputeContext*>(ctx);
                case CommandType::Copy:
                    return *(static_cast<VulkanTransferCommandBuffer*>(ctx)->command_buffer);
            }
        });

    std::pmr::vector<vk::Semaphore>          _wait_semaphores;
    std::pmr::vector<std::uint64_t>          _wait_values;
    std::pmr::vector<vk::PipelineStageFlags> _stage_flags;

    for (auto& [semaphore, value] : wait_semaphores) {
        _wait_semaphores.emplace_back(*static_cast<VulkanSemaphore*>(semaphore.get())->semaphore);
        _wait_values.emplace_back(value);
        // ! Please imporve me
        _stage_flags.emplace_back(vk::PipelineStageFlagBits::eAllCommands);
    }

    m_SubmitCount++;
    vk::StructureChain submit_info{
        vk::SubmitInfo{
            .waitSemaphoreCount   = static_cast<std::uint32_t>(_wait_semaphores.size()),
            .pWaitSemaphores      = _wait_semaphores.data(),
            .pWaitDstStageMask    = _stage_flags.data(),
            .commandBufferCount   = static_cast<std::uint32_t>(command_buffers.size()),
            .pCommandBuffers      = command_buffers.data(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &(*m_Semaphore->semaphore),
        },
        vk::TimelineSemaphoreSubmitInfo{
            .waitSemaphoreValueCount   = static_cast<std::uint32_t>(_wait_values.size()),
            .pWaitSemaphoreValues      = _wait_values.data(),
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues    = &m_SubmitCount,
        },
    };

    m_Queue.submit(submit_info.get());

    return {m_Semaphore, m_SubmitCount};
}

void VulkanCommandQueue::WaitIdle() {
    m_Queue.waitIdle();
}

}  // namespace hitagi::gfx