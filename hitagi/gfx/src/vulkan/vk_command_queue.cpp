#include "vk_command_queue.hpp"
#include "vk_device.hpp"
#include "vk_command_buffer.hpp"
#include "vk_sync.hpp"
#include "utils.hpp"

#include <hitagi/utils/soa.hpp>

#include <fmt/color.h>
#include <spdlog/logger.h>

#include <algorithm>

namespace hitagi::gfx {

VulkanCommandQueue::VulkanCommandQueue(VulkanDevice& device, CommandType type, std::string_view name, std::uint32_t queue_family_index)
    : CommandQueue(device, type, name),
      m_FamilyIndex(queue_family_index),
      m_Queue(device.GetDevice().getQueue(queue_family_index, magic_enum::enum_integer(type)))

{
    device.GetDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
        .objectType   = vk::ObjectType::eQueue,
        .objectHandle = get_vk_handle(m_Queue),
        .pObjectName  = m_Name.c_str(),
    });
}

void VulkanCommandQueue::Submit(std::pmr::vector<CommandContext*> contexts, std::pmr::vector<SemaphoreWaitPair> wait_semaphores, std::pmr::vector<SemaphoreWaitPair> signal_semaphores) {
    // make sure all context are same command type
    if (auto iter = std::find_if(
            contexts.begin(), contexts.end(),
            [this](auto ctx) { return ctx->GetType() != m_Type; });
        iter != contexts.end()) {
        m_Device.GetLogger()->error(
            "CommandContext type({}) mismatch({}). ignore the batch contexts!!",
            fmt::styled(magic_enum::enum_name(m_Type), fmt::fg(fmt::color::red)),
            fmt::styled(magic_enum::enum_name((*iter)->GetType()), fmt::fg(fmt::color::green)));

        contexts.clear();
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

    const auto _wait_semaphores   = convert_to_sao_vk_semaphore_wait_pairs(wait_semaphores);
    const auto _signal_semaphores = convert_to_sao_vk_semaphore_wait_pairs(signal_semaphores);

    std::pmr::vector<vk::PipelineStageFlags> _stage_flags(wait_semaphores.size(), vk::PipelineStageFlagBits::eAllCommands);

    vk::StructureChain submit_info{
        vk::SubmitInfo{
            .waitSemaphoreCount   = static_cast<std::uint32_t>(_wait_semaphores.size()),
            .pWaitSemaphores      = _wait_semaphores.elements<0>().data(),
            .pWaitDstStageMask    = _stage_flags.data(),
            .commandBufferCount   = static_cast<std::uint32_t>(command_buffers.size()),
            .pCommandBuffers      = command_buffers.data(),
            .signalSemaphoreCount = static_cast<std::uint32_t>(_signal_semaphores.size()),
            .pSignalSemaphores    = _signal_semaphores.elements<0>().data(),
        },
        vk::TimelineSemaphoreSubmitInfo{
            .waitSemaphoreValueCount   = static_cast<std::uint32_t>(_wait_semaphores.size()),
            .pWaitSemaphoreValues      = _wait_semaphores.elements<1>().data(),
            .signalSemaphoreValueCount = static_cast<std::uint32_t>(_signal_semaphores.size()),
            .pSignalSemaphoreValues    = _signal_semaphores.elements<1>().data(),
        },
    };

    m_Queue.submit(submit_info.get());
}

void VulkanCommandQueue::WaitIdle() {
    m_Queue.waitIdle();
}

}  // namespace hitagi::gfx