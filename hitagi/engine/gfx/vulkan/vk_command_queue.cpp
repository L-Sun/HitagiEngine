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

// A batch owns a pool and all buffers allocated from it. Wrappers keep the
// batch alive; individual wrapper destruction never frees a command buffer.
class VulkanCommandStorage {
public:
    struct Batch {
        Batch(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, std::uint32_t family);
        ~Batch();
        const vk::raii::CommandBuffer* AcquireBuffer();
        void PrepareSubmission(const std::shared_ptr<VulkanTimelineSemaphore>& fence);
        void Submitted(const std::shared_ptr<VulkanTimelineSemaphore>& fence, std::uint64_t value);
        bool Completed();
        void Reset();

        const vk::raii::Device& device;
        const std::uint32_t family;
        vk::raii::CommandPool pool;
        std::recursive_mutex recording_mutex;
        std::thread::id owner;
        bool sealed = false;
        std::size_t reset_count = 0;
    private:
        // Heap-owned buffers keep their addresses stable as a batch grows.
        std::pmr::vector<std::unique_ptr<vk::raii::CommandBuffer>> buffers;
        std::size_t next_buffer = 0;
        std::mutex submission_mutex;
        std::pmr::vector<std::pair<std::shared_ptr<VulkanTimelineSemaphore>, std::uint64_t>> submissions;
    };

    VulkanCommandStorage(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, std::uint32_t family)
        : m_Device(device), m_Callbacks(callbacks), m_Family(family) {}
    auto Acquire() -> std::shared_ptr<Batch>;
    void NewFrame();
private:
    const vk::raii::Device& m_Device;
    const vk::AllocationCallbacks& m_Callbacks;
    const std::uint32_t m_Family;
    std::mutex m_Mutex;
    std::pmr::vector<std::shared_ptr<Batch>> m_Batches;
};

VulkanCommandStorage::Batch& get_vulkan_command_batch(const CommandContext& context);

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

    void NewFrame() final;
    inline auto GetCommandStorage() const noexcept { return m_CommandStorage; }

    void InitializeTracyContext(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::raii::CommandPool& pool);

    inline auto GetFamilyIndex() const noexcept { return m_FamilyIndex; }
    inline auto GetNativeDevice() const noexcept { return m_NativeDevice; }
    inline auto GetNativeQueue() const noexcept { return m_Queue; }
    inline auto GetTracyCtx() const noexcept { return m_TracyCtx; }

private:
    std::uint32_t                      m_FamilyIndex;
    vk::Device                         m_NativeDevice;
    std::shared_ptr<VulkanNativeQueue> m_Queue;
    std::shared_ptr<VulkanCommandStorage> m_CommandStorage;
    std::shared_ptr<VulkanTimelineSemaphore> m_Completion;
    std::uint64_t m_SubmitCount = 0;
    TracyVkCtx                         m_TracyCtx = nullptr;
    std::shared_ptr<spdlog::logger>    m_Logger;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanCommandQueue::VulkanCommandQueue(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, const vk::AllocationCallbacks& callbacks, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name, std::shared_ptr<VulkanNativeQueue> native_queue)
    : CommandQueue(type, name), m_Logger(std::move(logger)), m_FamilyIndex(native_queue->family_index), m_NativeDevice(*device), m_Queue(std::move(native_queue)), m_CommandStorage(std::make_shared<VulkanCommandStorage>(device, callbacks, m_FamilyIndex)), m_Completion(std::make_shared<VulkanTimelineSemaphore>(device, callbacks, 0, "CommandPoolCompletion"))

{
    // Native queue names must not be overwritten by each logical wrapper.
    // Tracy initialization uses a separate, short-lived pool.
    vk::raii::CommandPool setup_pool(device, vk::CommandPoolCreateInfo{.queueFamilyIndex = m_FamilyIndex}, callbacks);
    InitializeTracyContext(device, physical_device, setup_pool);
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

    for (const CommandContext& context : contexts) {
        auto& batch = get_vulkan_command_batch(context);
        if (batch.family != m_FamilyIndex || *batch.device != m_NativeDevice)
            throw std::invalid_argument("Command buffer belongs to another device or queue family");
        batch.PrepareSubmission(m_Completion);
    }
    // Reserve the final signal before taking pointers into this vector.
    signal_semaphore_infos.emplace_back(vk::SemaphoreSubmitInfo{
        .semaphore = *m_Completion->timeline_semaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    });
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
    signal_semaphore_infos.back().value = m_SubmitCount + 1;
    m_Queue->queue.submit2(submit_info);
    ++m_SubmitCount;
    // All storage for completion records was allocated before submit2. Failure
    // before submission leaves zero-valued (already completed) records only.
    for (const CommandContext& context : contexts)
        get_vulkan_command_batch(context).Submitted(m_Completion, m_SubmitCount);
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

void VulkanCommandQueue::NewFrame() {
    m_CommandStorage->NewFrame();
}

VulkanCommandStorage::Batch::Batch(const vk::raii::Device& device, const vk::AllocationCallbacks& callbacks, std::uint32_t family)
    : device(device), family(family), pool(device, vk::CommandPoolCreateInfo{
          .flags = vk::CommandPoolCreateFlagBits::eTransient,
          .queueFamilyIndex = family,
      }, callbacks), owner(std::this_thread::get_id()) {}

VulkanCommandStorage::Batch::~Batch() {
    // The owning storage can disappear while a wrapper still retains this batch.
    for (const auto& [fence, value] : submissions)
        if (fence->GetCurrentValue() < value && !fence->Wait(value)) std::terminate();
}

auto VulkanCommandStorage::Batch::AcquireBuffer() -> const vk::raii::CommandBuffer* {
    std::scoped_lock lock(recording_mutex);
    if (next_buffer == buffers.size()) {
        auto allocated = vk::raii::CommandBuffers(device, vk::CommandBufferAllocateInfo{
            .commandPool = *pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        buffers.emplace_back(std::make_unique<vk::raii::CommandBuffer>(std::move(allocated.front())));
    }
    return buffers[next_buffer++].get();
}

void VulkanCommandStorage::Batch::PrepareSubmission(const std::shared_ptr<VulkanTimelineSemaphore>& fence) {
    std::scoped_lock lock(submission_mutex);
    for (const auto& record : submissions) if (record.first == fence) return;
    submissions.emplace_back(fence, 0);
}

void VulkanCommandStorage::Batch::Submitted(const std::shared_ptr<VulkanTimelineSemaphore>& fence, std::uint64_t value) {
    std::scoped_lock lock(submission_mutex);
    for (auto& record : submissions) {
        if (record.first == fence) {
            record.second = (std::max)(record.second, value);
            return;
        }
    }
    std::terminate(); // PrepareSubmission is mandatory before issuing GPU work.
}

bool VulkanCommandStorage::Batch::Completed() {
    std::scoped_lock lock(submission_mutex);
    return std::ranges::all_of(submissions, [](const auto& record) {
        return record.first->GetCurrentValue() >= record.second;
    });
}

void VulkanCommandStorage::Batch::Reset() {
    ZoneScopedNS("Vulkan.CommandPool.Reset", 0);
    // Caller owns the sole batch reference: no wrapper can record or submit.
    pool.reset();
    next_buffer = 0;
    submissions.clear();
    ++reset_count;
}

auto VulkanCommandStorage::Acquire() -> std::shared_ptr<Batch> {
    std::scoped_lock lock(m_Mutex);
    const auto thread = std::this_thread::get_id();
    for (auto& batch : m_Batches) {
        if (!batch->sealed && batch->owner == thread) {
            // Also support utility callers that never advance NewFrame.
            if (batch.use_count() == 1 && batch->Completed()) batch->Reset();
            return batch;
        }
    }
    for (auto& batch : m_Batches) {
        if (batch->sealed && batch.use_count() == 1 && batch->Completed()) {
            batch->Reset();
            batch->sealed = false;
            batch->owner = thread;
            return batch;
        }
    }
    auto batch = std::make_shared<Batch>(m_Device, m_Callbacks, m_Family);
    m_Batches.emplace_back(batch);
    return batch;
}

void VulkanCommandStorage::NewFrame() {
    std::scoped_lock lock(m_Mutex);
    // Seal without waiting. Acquire resets only a fully returned, completed batch.
    for (auto& batch : m_Batches) batch->sealed = true;
}

}  // namespace hitagi::gfx
