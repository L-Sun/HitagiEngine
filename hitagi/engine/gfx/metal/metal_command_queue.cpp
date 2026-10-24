module;
#include "interop/tracy_macros.hpp"

export module gfx.metal:command_queue;
import interop.metal;
import interop.magic_enum;
import interop.spdlog;
import interop.tracy;

import std;
import gfx.base;
import :types;
import :utils;
import :command_buffer;
import :sync;

export namespace hitagi::gfx {

class MetalCommandQueue final : public CommandQueue {
public:
    MetalCommandQueue(const ns::SharedPtr<mtl::Device>& device, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name);

    ~MetalCommandQueue() final;

    void Submit(std::span<const std::reference_wrapper<const CommandContext>> contexts,
                std::span<const FenceWaitInfo>                                wait_fences   = {},
                std::span<const FenceSignalInfo>                              signal_fences = {}) final;
    void WaitIdle() final;
    void NewFrame() final;

    inline auto  GetNativeDevice() const noexcept { return &m_Device; }
    inline auto& GetMetalQueue() const noexcept { return *m_Queue.get(); }

private:
    struct Submission {
        std::uint64_t                                                  value;
        std::pmr::vector<std::shared_ptr<const MetalCommandRecording>> recordings;
        std::pmr::vector<ns::SharedPtr<mtl::SharedEvent>>              events;
    };
    void                              RetireCompleted();
    std::mutex                        m_Mutex;
    std::pmr::deque<Submission>       m_Pending;
    const ns::SharedPtr<mtl::Device>& m_Device;
    std::shared_ptr<spdlog::logger>   m_Logger;
    ns::SharedPtr<mtl4::CommandQueue> m_Queue;
    ns::SharedPtr<mtl::SharedEvent>   m_Completion;
    std::uint64_t                     m_Submission = 0;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MetalCommandQueue::MetalCommandQueue(const ns::SharedPtr<mtl::Device>& device, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name)
    : CommandQueue(type, name), m_Device(device), m_Logger(std::move(logger)), m_Queue(ns::TransferPtr(device->newMTL4CommandQueue())), m_Completion(ns::TransferPtr(device->newSharedEvent())) {
    check_metal(m_Logger, m_Queue.get() != nullptr, "Create Metal 4 command queue failed");
    check_metal(m_Logger, m_Completion.get() != nullptr, "Create Metal completion event failed");
    m_Logger->trace("Create Metal 4 command queue({})", name);
}

MetalCommandQueue::~MetalCommandQueue() { WaitIdle(); }

void MetalCommandQueue::Submit(std::span<const std::reference_wrapper<const CommandContext>> contexts,
                               std::span<const FenceWaitInfo>                                wait_fences,
                               std::span<const FenceSignalInfo>                              signal_fences) {
    ZoneScoped;
    std::scoped_lock lock(m_Mutex);
    // Validate and retain the complete batch before enqueuing any GPU operation.
    Submission                                   submission{.value = m_Submission + 1};
    std::pmr::vector<const mtl4::CommandBuffer*> buffers;
    for (const CommandContext& context : contexts) {
        if (context.GetType() != m_Type) {
            m_Logger->warn("CommandContext type({}) does not match queue type({}); submission skipped", magic_enum::enum_name(context.GetType()), magic_enum::enum_name(m_Type));
            return;
        }
        const auto& buffer = dynamic_cast<const MetalCommandBuffer&>(context);
        check_metal(m_Logger, buffer.GetNativeDevice() == &m_Device, "Metal command buffer belongs to another device");
        auto recording = buffer.GetRecording();
        check_metal(m_Logger, recording && recording->ended, "Metal command buffer has not ended");
        buffers.push_back(recording->command_buffer.get());
        submission.recordings.push_back(std::move(recording));
    }
    const auto retain_fence = [&](Fence& source) {
        const auto& fence = dynamic_cast<const MetalFence&>(source);
        check_metal(m_Logger, fence.GetNativeDevice() == &m_Device, "Metal fence belongs to another device");
        submission.events.push_back(fence.GetEvent());
    };
    for (const auto& wait : wait_fences) retain_fence(wait.fence);
    for (const auto& signal : signal_fences) retain_fence(signal.fence);
    RetireCompleted();
    m_Pending.push_back(std::move(submission));
    const auto& events = m_Pending.back().events;
    for (std::size_t i = 0; i < wait_fences.size(); ++i) {
        if (wait_fences[i].value != 0) m_Queue->wait(events[i].get(), wait_fences[i].value);
    }
    if (!buffers.empty()) m_Queue->commit(buffers.data(), buffers.size());
    for (std::size_t i = 0; i < signal_fences.size(); ++i)
        m_Queue->signalEvent(events[wait_fences.size() + i].get(), signal_fences[i].value);
    m_Queue->signalEvent(m_Completion.get(), ++m_Submission);
}

void MetalCommandQueue::RetireCompleted() {
    const auto completed = m_Completion->signaledValue();
    while (!m_Pending.empty() && m_Pending.front().value <= completed) m_Pending.pop_front();
}

void MetalCommandQueue::WaitIdle() {
    ZoneScoped;
    std::scoped_lock lock(m_Mutex);
    if (m_Submission != 0)
        check_metal(m_Logger, m_Completion->waitUntilSignaledValue(m_Submission, 60000), "Metal GPU completion timed out");
    RetireCompleted();
}

void MetalCommandQueue::NewFrame() {
    std::scoped_lock lock(m_Mutex);
    RetireCompleted();
}

}  // namespace hitagi::gfx
