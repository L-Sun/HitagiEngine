module;
#include "interop/win32_macros.hpp"
#include "interop/tracy_macros.hpp"

export module gfx.dx12:command_queue;
#ifdef _WIN32
import interop.win32;
#endif
import interop.fmt;
import interop.spdlog;
import interop.tracy.dx12;
import interop.tracy;
import interop.dx12;
import interop.magic_enum;

import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :sync;
import :command_list;
import :utils;
import :resource;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

class DX12CommandQueue : public CommandQueue {
public:
    DX12CommandQueue(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name);
    ~DX12CommandQueue() override;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final;

    void WaitIdle() final;
    void NewFrame() final;

    inline auto GetDX12Queue() const noexcept { return m_Queue; }
    inline auto GetCommandStorage() const noexcept { return m_CommandStorage; }
    inline auto GetTracyCtx() const noexcept { return m_TracyCtx; }

private:
    std::shared_ptr<DX12CommandStorage> m_CommandStorage = std::make_shared<DX12CommandStorage>();
    std::mutex                          m_SubmitMutex;
    std::shared_ptr<spdlog::logger>     m_Logger;
    ComPtr<ID3D12CommandQueue>          m_Queue;
    DX12Fence                           m_Fence;
    std::uint64_t                       m_SubmitCount = 0;
    TracyD3D12Ctx                       m_TracyCtx    = nullptr;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

DX12CommandQueue::DX12CommandQueue(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, CommandType type, std::string_view name)
    : CommandQueue(type, name), m_Logger(std::move(logger)), m_Fence(device, m_Logger, 0, std::format("{}_Fence", name)) {
    logger = m_Logger;

    const D3D12_COMMAND_QUEUE_DESC desc{
        .Type     = to_d3d_command_type(type),
        .Priority = 0,
        .Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };

    logger->trace("Creating Command Queue: {}", fmt::styled(name, fmt::fg(fmt::color::green)));
    if (interop::failed(device.CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue)))) {
        const auto error_message = fmt::format("Failed to create Command Queue({})", fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    m_Queue->SetName(std::wstring(name.begin(), name.end()).c_str());
    m_TracyCtx = TracyD3D12Context(&device, m_Queue.Get());
    TracyD3D12ContextName(m_TracyCtx, m_Name.data(), static_cast<std::uint16_t>(m_Name.size()));
}

DX12CommandQueue::~DX12CommandQueue() {
    WaitIdle();
    TracyD3D12Destroy(m_TracyCtx);
}

void DX12CommandQueue::Submit(std::span<const std::reference_wrapper<const CommandContext>> contexts,
                              std::span<const FenceWaitInfo>                                wait_fences,
                              std::span<const FenceSignalInfo>                              signal_fences) {
    std::scoped_lock submit_lock(m_SubmitMutex);
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

    std::pmr::vector<ID3D12CommandList*> command_lists;
    std::transform(
        contexts.begin(), contexts.end(),
        std::back_inserter(command_lists),
        [](const CommandContext& ctx) -> ID3D12CommandList* {
            switch (ctx.GetType()) {
                case CommandType::Graphics:
                    return dynamic_cast<const DX12GraphicsCommandList&>(ctx).storage_entry->list.Get();
                case CommandType::Compute:
                    return dynamic_cast<const DX12ComputeCommandList&>(ctx).storage_entry->list.Get();
                case CommandType::Copy:
                    return dynamic_cast<const DX12CopyCommandList&>(ctx).storage_entry->list.Get();
                default:
                    utils::unreachable();
            }
        });

    // Record completion ownership before submitting GPU work (allocation may throw).
    const auto completion_value = m_SubmitCount + 1;
    try {
        for (const CommandContext& context : contexts) {
            switch (context.GetType()) {
                case CommandType::Graphics: {
                    const auto& native = dynamic_cast<const DX12GraphicsCommandList&>(context);
                    if (!native.closed) throw std::logic_error("Cannot submit an open command list");
                    native.storage->Submitted(native.storage_entry.get(), m_Fence.GetFence(), completion_value);
                    break;
                }
                case CommandType::Compute: {
                    const auto& native = dynamic_cast<const DX12ComputeCommandList&>(context);
                    if (!native.closed) throw std::logic_error("Cannot submit an open command list");
                    native.storage->Submitted(native.storage_entry.get(), m_Fence.GetFence(), completion_value);
                    break;
                }
                case CommandType::Copy: {
                    const auto& native = dynamic_cast<const DX12CopyCommandList&>(context);
                    if (!native.closed) throw std::logic_error("Cannot submit an open command list");
                    native.storage->Submitted(native.storage_entry.get(), m_Fence.GetFence(), completion_value);
                    break;
                }
                default:
                    utils::unreachable();
            }
        }
    } catch (...) {
        // No GPU work from this call was issued; complete any prepared stamps.
        m_Queue->Signal(m_Fence.GetFence().Get(), ++m_SubmitCount);
        throw;
    }

    for (const auto& wait_fence : wait_fences) {
        if (wait_fence.value == 0) continue;
        const auto& fence = dynamic_cast<DX12Fence&>(wait_fence.fence);
        m_Queue->Wait(fence.GetFence().Get(), wait_fence.value);
    }

    if (!command_lists.empty()) {
        m_Queue->ExecuteCommandLists(command_lists.size(), command_lists.data());
        TracyD3D12Collect(m_TracyCtx);
    }

    for (const auto& signal_fence : signal_fences) {
        const auto& fence = dynamic_cast<DX12Fence&>(signal_fence.fence);
        m_Queue->Signal(fence.GetFence().Get(), signal_fence.value);
    }
    if (interop::failed(m_Queue->Signal(m_Fence.GetFence().Get(), ++m_SubmitCount)))
        throw std::runtime_error("Failed to signal command retirement fence");
}

void DX12CommandQueue::WaitIdle() {
    if (m_SubmitCount == 0) return;
    m_Fence.Wait(m_SubmitCount);
}

void DX12CommandQueue::NewFrame() {
    m_CommandStorage->CollectCompleted();
    TracyD3D12NewFrame(m_TracyCtx);
}

}  // namespace hitagi::gfx
