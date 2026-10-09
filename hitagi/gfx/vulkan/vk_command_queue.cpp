module;
#include "interop/tracy_macros.hpp"

export module gfx.vulkan:command_queue;
import interop.fmt;
import interop.spdlog;
import interop.tracy.vulkan;
import interop.tracy;
import interop.vulkan;
import interop.magic_enum;

import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :sync;
import :command_buffer;
import :utils;
import :resource;
import :configs;

export namespace hitagi::gfx {

class VulkanCommandQueue final : public CommandQueue {
public:
    VulkanCommandQueue(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::AllocationCallbacks& callbacks, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name, std::uint32_t queue_family_index);
    ~VulkanCommandQueue() final;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> commands,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final;

    void WaitIdle() final;

    inline auto& GetCommandPool() const noexcept { return m_CommandPool; }

    void InitializeTracyContext(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::raii::CommandPool& pool);

    inline auto  GetFamilyIndex() const noexcept { return m_FamilyIndex; }
    inline auto  GetNativeDevice() const noexcept { return m_NativeDevice; }
    inline auto& GetVkQueue() const noexcept { return m_Queue; }
    inline auto  GetTracyCtx() const noexcept { return m_TracyCtx; }

private:
    std::uint32_t                   m_FamilyIndex;
    vk::Device                      m_NativeDevice;
    vk::raii::Queue                 m_Queue;
    vk::raii::CommandPool           m_CommandPool;
    TracyVkCtx                      m_TracyCtx = nullptr;
    std::shared_ptr<spdlog::logger> m_Logger;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanCommandQueue::VulkanCommandQueue(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::AllocationCallbacks& callbacks, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name, std::uint32_t queue_family_index)
    : CommandQueue(type, name), m_Logger(std::move(logger)), m_FamilyIndex(queue_family_index), m_NativeDevice(*device), m_Queue(device.getQueue(queue_family_index, magic_enum::enum_integer(type))), m_CommandPool(device, vk::CommandPoolCreateInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = queue_family_index}, callbacks)

{
    create_vk_debug_object_info(m_Queue, m_Name, device);
    InitializeTracyContext(device, physical_device, m_CommandPool);
}

VulkanCommandQueue::~VulkanCommandQueue() {
    WaitIdle();
    TracyVkDestroy(m_TracyCtx);
}

void VulkanCommandQueue::InitializeTracyContext(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::raii::CommandPool& pool) {
    auto setup_command_buffer = std::move(vk::raii::CommandBuffers(
                                              device,
                                              vk::CommandBufferAllocateInfo{
                                                  .commandPool        = *pool,
                                                  .level              = vk::CommandBufferLevel::ePrimary,
                                                  .commandBufferCount = 1,
                                              })
                                              .front());

    m_TracyCtx = TracyVkContext(*physical_device, *device, *m_Queue, *setup_command_buffer);
    TracyVkContextName(m_TracyCtx, m_Name.data(), static_cast<std::uint16_t>(m_Name.size()));
}

void VulkanCommandQueue::Submit(std::span<const std::reference_wrapper<const CommandContext>> contexts,
                                std::span<const FenceWaitInfo>                                wait_fences,
                                std::span<const FenceSignalInfo>                              signal_fences) {
    // make sure all context are same command type
    if (auto iter = std::find_if(
            contexts.begin(), contexts.end(),
            [this](const CommandContext& ctx) { return ctx.GetType() != m_Type; });
        iter != contexts.end()) {
        m_Logger->warn(
            "CommandContext type({}) mismatch({}). Do nothing!!!",
            fmt::styled(magic_enum::enum_name(m_Type), fmt::fg(fmt::color::red)),
            fmt::styled(magic_enum::enum_name((*iter).get().GetType()), fmt::fg(fmt::color::green)));

        return;
    }

    const auto cmd_buffer_infos =
        contexts |
        std::ranges::views::transform([](const CommandContext& ctx) -> vk::CommandBufferSubmitInfo {
            vk::CommandBuffer cmd_buf;
            switch (ctx.GetType()) {
                case CommandType::Graphics:
                    cmd_buf = *(dynamic_cast<const VulkanGraphicsCommandBuffer&>(ctx).command_buffer);
                    break;
                case CommandType::Compute:
                    cmd_buf = *(dynamic_cast<const VulkanComputeCommandBuffer&>(ctx).command_buffer);
                    break;
                case CommandType::Copy:
                    cmd_buf = *(dynamic_cast<const VulkanTransferCommandBuffer&>(ctx).command_buffer);
                    break;
                default:
                    utils::unreachable();
            }
            return {.commandBuffer = cmd_buf};
        }) |
        std::ranges::to<std::pmr::vector<vk::CommandBufferSubmitInfo>>();

    auto wait_semaphore_infos =
        wait_fences |
        std::ranges::views::transform([](const auto& wait_info) -> vk::SemaphoreSubmitInfo {
            return {
                .semaphore = *dynamic_cast<const VulkanTimelineSemaphore&>(wait_info.fence).timeline_semaphore,
                .value     = wait_info.value,
                .stageMask = to_vk_pipeline_stage2(wait_info.stage),
            };
        }) |
        std::ranges::to<std::pmr::vector<vk::SemaphoreSubmitInfo>>();

    auto signal_semaphore_infos =
        signal_fences |
        std::ranges::views::transform([](const auto& signal_info) -> vk::SemaphoreSubmitInfo {
            return {
                .semaphore = *dynamic_cast<const VulkanTimelineSemaphore&>(signal_info.fence).timeline_semaphore,
                .value     = signal_info.value,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            };
        }) |
        std::ranges::to<std::pmr::vector<vk::SemaphoreSubmitInfo>>();

    for (const CommandContext& ctx : contexts) {
        if (ctx.GetType() == CommandType::Graphics) {
            auto& gfx_ctx = dynamic_cast<const VulkanGraphicsCommandBuffer&>(ctx);
            if (gfx_ctx.swap_chain_image_available_semaphore) {
                wait_semaphore_infos.emplace_back(vk::SemaphoreSubmitInfo{
                    .semaphore = **gfx_ctx.swap_chain_image_available_semaphore,
                    .value     = 0,
                    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                });
            }

            for (const auto& swapchain_presentable_semaphore : gfx_ctx.swap_chain_presentable_semaphores) {
                signal_semaphore_infos.emplace_back(vk::SemaphoreSubmitInfo{
                    .semaphore = **swapchain_presentable_semaphore,
                    .value     = 0,
                    .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
                });
            }
        } else if (ctx.GetType() == CommandType::Copy) {
            auto& transfer_ctx = dynamic_cast<const VulkanTransferCommandBuffer&>(ctx);
            if (transfer_ctx.swap_chain_image_available_semaphore) {
                wait_semaphore_infos.emplace_back(vk::SemaphoreSubmitInfo{
                    .semaphore = **transfer_ctx.swap_chain_image_available_semaphore,
                    .value     = 0,
                    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                });
            }

            for (const auto& swapchain_presentable_semaphore : transfer_ctx.swap_chain_presentable_semaphores) {
                signal_semaphore_infos.emplace_back(vk::SemaphoreSubmitInfo{
                    .semaphore = **swapchain_presentable_semaphore,
                    .value     = 0,
                    .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
                });
            }
        }
    }

    const vk::SubmitInfo2 submit_info{
        .waitSemaphoreInfoCount   = static_cast<std::uint32_t>(wait_semaphore_infos.size()),
        .pWaitSemaphoreInfos      = wait_semaphore_infos.data(),
        .commandBufferInfoCount   = static_cast<std::uint32_t>(cmd_buffer_infos.size()),
        .pCommandBufferInfos      = cmd_buffer_infos.data(),
        .signalSemaphoreInfoCount = static_cast<std::uint32_t>(signal_semaphore_infos.size()),
        .pSignalSemaphoreInfos    = signal_semaphore_infos.data(),
    };

    m_Queue.submit2(submit_info);
}

void VulkanCommandQueue::WaitIdle() {
    m_Queue.waitIdle();
}

}  // namespace hitagi::gfx
