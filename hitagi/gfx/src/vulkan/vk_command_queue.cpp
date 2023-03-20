#include "vk_command_queue.hpp"
#include "vk_device.hpp"
#include "vk_sync.hpp"
#include "vk_command_buffer.hpp"
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
    create_vk_debug_object_info(m_Queue, m_Name, device.GetDevice());
}

void VulkanCommandQueue::Submit(
    std::pmr::vector<CommandContext*>                         contexts,
    std::pmr::vector<std::reference_wrapper<const Semaphore>> wait_semaphores,
    std::pmr::vector<std::reference_wrapper<const Semaphore>> signal_semaphores,
    utils::optional_ref<const Fence>                          fence) {
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

    std::pmr::vector<vk::PipelineStageFlags> _stage_flags(wait_semaphores.size(), vk::PipelineStageFlagBits::eAllCommands);

    auto extract_vk_semaphpre_fn = [](const Semaphore& semaphore) { return *static_cast<const VulkanSemaphore&>(semaphore).semaphore; };

    std::pmr::vector<vk::Semaphore> wait_vk_semaphores;
    std::pmr::vector<vk::Semaphore> signal_vk_semaphores;
    std::transform(wait_semaphores.begin(), wait_semaphores.end(), std::back_inserter(wait_vk_semaphores), extract_vk_semaphpre_fn);
    std::transform(signal_semaphores.begin(), signal_semaphores.end(), std::back_inserter(signal_vk_semaphores), extract_vk_semaphpre_fn);

    m_Queue.submit(vk::SubmitInfo{
                       .waitSemaphoreCount   = static_cast<std::uint32_t>(wait_vk_semaphores.size()),
                       .pWaitSemaphores      = wait_vk_semaphores.data(),
                       .pWaitDstStageMask    = _stage_flags.data(),
                       .commandBufferCount   = static_cast<std::uint32_t>(command_buffers.size()),
                       .pCommandBuffers      = command_buffers.data(),
                       .signalSemaphoreCount = static_cast<std::uint32_t>(signal_vk_semaphores.size()),
                       .pSignalSemaphores    = signal_vk_semaphores.data(),
                   },
                   fence ? *static_cast<const VulkanFence&>(fence->get()).fence : vk::Fence{});
}

void VulkanCommandQueue::WaitIdle() {
    m_Queue.waitIdle();
}

}  // namespace hitagi::gfx