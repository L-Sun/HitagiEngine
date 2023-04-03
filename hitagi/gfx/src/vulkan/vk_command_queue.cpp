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

void VulkanCommandQueue::Submit(const std::pmr::vector<CommandContext*>& contexts,
                                const std::pmr::vector<FenceWaitInfo>&   wait_fences,
                                const std::pmr::vector<FenceSignalInfo>& signal_fences) {
    // make sure all context are same command type
    if (auto iter = std::find_if(
            contexts.begin(), contexts.end(),
            [this](auto ctx) { return ctx->GetType() != m_Type; });
        iter != contexts.end()) {
        m_Device.GetLogger()->warn(
            "CommandContext type({}) mismatch({}). Do nothing!!!",
            fmt::styled(magic_enum::enum_name(m_Type), fmt::fg(fmt::color::red)),
            fmt::styled(magic_enum::enum_name((*iter)->GetType()), fmt::fg(fmt::color::green)));

        return;
    }

    std::pmr::vector<vk::CommandBuffer> command_buffers;
    std::transform(
        contexts.begin(), contexts.end(),
        std::back_inserter(command_buffers),
        [](auto ctx) -> vk::CommandBuffer {
            switch (ctx->GetType()) {
                case CommandType::Graphics:
                    return *(static_cast<VulkanGraphicsCommandBuffer*>(ctx)->command_buffer);
                case CommandType::Compute:
                    return *(static_cast<VulkanComputeCommandBuffer*>(ctx)->command_buffer);
                case CommandType::Copy:
                    return *(static_cast<VulkanTransferCommandBuffer*>(ctx)->command_buffer);
            }
        });

    std::pmr::vector<vk::Semaphore>          wait_vk_semaphores;
    std::pmr::vector<std::uint64_t>          wait_values;
    std::pmr::vector<vk::PipelineStageFlags> wait_stage;
    for (const auto& wait_info : wait_fences) {
        wait_vk_semaphores.emplace_back(*static_cast<const VulkanTimelineSemaphore&>(wait_info.fence).timeline_semaphore);
        wait_values.emplace_back(wait_info.value);
        wait_stage.emplace_back(to_vk_pipeline_stage1(wait_info.stage));
    }

    std::pmr::vector<vk::Semaphore> signal_vk_semaphores;
    std::pmr::vector<std::uint64_t> signal_values;
    for (const auto& signal_info : signal_fences) {
        signal_vk_semaphores.emplace_back(*static_cast<const VulkanTimelineSemaphore&>(signal_info.fence).timeline_semaphore);
        signal_values.emplace_back(signal_info.value);
    }

    for (auto ctx : contexts) {
        if (ctx->GetType() == CommandType::Graphics) {
            auto gfx_ctx = static_cast<VulkanGraphicsCommandBuffer*>(ctx);
            if (gfx_ctx->swap_chain_image_available_semaphore) {
                wait_vk_semaphores.emplace_back(**gfx_ctx->swap_chain_image_available_semaphore);
                wait_values.emplace_back(0);
            }

            if (gfx_ctx->swap_chain_presentable_semaphore) {
                signal_vk_semaphores.emplace_back(**gfx_ctx->swap_chain_presentable_semaphore);
                signal_values.emplace_back(0);
            }
        }
    }

    const vk::StructureChain submit_info{
        vk::SubmitInfo{
            .waitSemaphoreCount   = static_cast<std::uint32_t>(wait_vk_semaphores.size()),
            .pWaitSemaphores      = wait_vk_semaphores.data(),
            .pWaitDstStageMask    = wait_stage.data(),
            .commandBufferCount   = static_cast<std::uint32_t>(command_buffers.size()),
            .pCommandBuffers      = command_buffers.data(),
            .signalSemaphoreCount = static_cast<std::uint32_t>(signal_vk_semaphores.size()),
            .pSignalSemaphores    = signal_vk_semaphores.data(),
        },
        vk::TimelineSemaphoreSubmitInfo{
            .waitSemaphoreValueCount   = static_cast<std::uint32_t>(wait_values.size()),
            .pWaitSemaphoreValues      = wait_values.data(),
            .signalSemaphoreValueCount = static_cast<std::uint32_t>(signal_values.size()),
            .pSignalSemaphoreValues    = signal_values.data(),
        }};

    m_Queue.submit(submit_info.get());
}

void VulkanCommandQueue::WaitIdle() {
    m_Queue.waitIdle();
}

}  // namespace hitagi::gfx