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
import :utils;
import :configs;

export namespace hitagi::gfx {

// One state per native queue in a device, shared even across CommandQueues groups.
struct VulkanNativeQueue {
    VulkanNativeQueue(const vk::raii::Device& device, std::uint32_t family, std::uint32_t index)
        : queue(device.getQueue(family, index)), family_index(family), queue_index(index) {}

    vk::raii::Queue     queue;
    const std::uint32_t family_index;
    const std::uint32_t queue_index;
    std::mutex          mutex;
};

inline void wait_vulkan_device_idle(const vk::raii::Device& device, std::span<const std::shared_ptr<VulkanNativeQueue>> queues) {
    // Every caller takes the device's unique queues in creation order.
    std::pmr::vector<std::unique_lock<std::mutex>> locks;
    locks.reserve(queues.size());
    for (const auto& queue : queues) locks.emplace_back(queue->mutex);
    device.waitIdle();
}

// Defined with the concrete command buffers, keeping this partition independent of resources.
void append_vulkan_submission(const CommandContext& context,
                              std::pmr::vector<vk::CommandBufferSubmitInfo>& commands,
                              std::pmr::vector<vk::SemaphoreSubmitInfo>& waits,
                              std::pmr::vector<vk::SemaphoreSubmitInfo>& signals);

class VulkanCommandQueue final : public CommandQueue {
public:
    VulkanCommandQueue(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::AllocationCallbacks& callbacks, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name, std::shared_ptr<VulkanNativeQueue> native_queue);
    ~VulkanCommandQueue() final;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> commands,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final;

    void WaitIdle() final;

    inline auto& GetCommandPool() const noexcept { return m_CommandPool; }

    void InitializeTracyContext(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::raii::CommandPool& pool);

    inline auto GetFamilyIndex() const noexcept { return m_FamilyIndex; }
    inline auto GetNativeDevice() const noexcept { return m_NativeDevice; }
    inline auto GetNativeQueue() const noexcept { return m_Queue; }
    inline auto GetTracyCtx() const noexcept { return m_TracyCtx; }

private:
    std::uint32_t                      m_FamilyIndex;
    vk::Device                         m_NativeDevice;
    std::shared_ptr<VulkanNativeQueue> m_Queue;
    vk::raii::CommandPool              m_CommandPool;
    TracyVkCtx                         m_TracyCtx = nullptr;
    std::shared_ptr<spdlog::logger>    m_Logger;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanCommandQueue::VulkanCommandQueue(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::AllocationCallbacks& callbacks, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name, std::shared_ptr<VulkanNativeQueue> native_queue)
    : CommandQueue(type, name), m_Logger(std::move(logger)), m_FamilyIndex(native_queue->family_index), m_NativeDevice(*device), m_Queue(std::move(native_queue)), m_CommandPool(device, vk::CommandPoolCreateInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = m_FamilyIndex}, callbacks)

{
    // Native queue names must not be overwritten by each logical wrapper.
    InitializeTracyContext(device, physical_device, m_CommandPool);
}

VulkanCommandQueue::~VulkanCommandQueue() {
    WaitIdle();
    TracyVkDestroy(m_TracyCtx);
}

void VulkanCommandQueue::InitializeTracyContext(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::raii::CommandPool& pool) {
    auto setup_command_buffer = std::move(
        vk::raii::CommandBuffers(
            device,
            vk::CommandBufferAllocateInfo{
                .commandPool        = *pool,
                .level              = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            })
            .front());

    std::scoped_lock lock{m_Queue->mutex};
    m_TracyCtx = TracyVkContext(*physical_device, *device, *m_Queue->queue, *setup_command_buffer);
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

    std::pmr::vector<vk::CommandBufferSubmitInfo> cmd_buffer_infos;
    cmd_buffer_infos.reserve(contexts.size());

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
        append_vulkan_submission(ctx, cmd_buffer_infos, wait_semaphore_infos, signal_semaphore_infos);
    }

    const vk::SubmitInfo2 submit_info{
        .waitSemaphoreInfoCount   = static_cast<std::uint32_t>(wait_semaphore_infos.size()),
        .pWaitSemaphoreInfos      = wait_semaphore_infos.data(),
        .commandBufferInfoCount   = static_cast<std::uint32_t>(cmd_buffer_infos.size()),
        .pCommandBufferInfos      = cmd_buffer_infos.data(),
        .signalSemaphoreInfoCount = static_cast<std::uint32_t>(signal_semaphore_infos.size()),
        .pSignalSemaphoreInfos    = signal_semaphore_infos.data(),
    };

#ifdef TRACY_ENABLE
    const auto lock_start = std::chrono::steady_clock::now();
#endif
    std::scoped_lock lock{m_Queue->mutex};
#ifdef TRACY_ENABLE
    const auto submit_start = std::chrono::steady_clock::now();
#endif
    m_Queue->queue.submit2(submit_info);
#ifdef TRACY_ENABLE
    const auto submit_end = std::chrono::steady_clock::now();
    interop::plot_profiling_value("Vulkan Queue Lock (ms)", std::chrono::duration<double, std::milli>(submit_start - lock_start).count());
    interop::plot_profiling_value("Vulkan Queue Submit (ms)", std::chrono::duration<double, std::milli>(submit_end - submit_start).count());
#endif
}

void VulkanCommandQueue::WaitIdle() {
    std::scoped_lock lock{m_Queue->mutex};
    m_Queue->queue.waitIdle();
}

}  // namespace hitagi::gfx
