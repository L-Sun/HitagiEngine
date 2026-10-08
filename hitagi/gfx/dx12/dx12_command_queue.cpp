module;
#include <d3d12.h>
#include <wrl.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyD3D12.hpp>
#include <spdlog/logger.h>
#include <fmt/color.h>
#include <d3dx12/d3dx12.h>

export module gfx.dx12:command_queue;
import std;
import core;
import utils;
import math;
import gfx.base;
import magic_enum;
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
    inline auto GetTracyCtx() const noexcept { return m_TracyCtx; }

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    ComPtr<ID3D12CommandQueue>      m_Queue;
    DX12Fence                       m_Fence;
    std::uint64_t                   m_SubmitCount = 0;
    TracyD3D12Ctx                   m_TracyCtx    = nullptr;
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
    if (FAILED(device.CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue)))) {
        const auto error_message = fmt::format("Failed to create Command Queue({})", fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    m_Queue->SetName(std::wstring(name.begin(), name.end()).c_str());
    m_TracyCtx = TracyD3D12Context(&device, m_Queue.Get());
    TracyD3D12ContextName(m_TracyCtx, m_Name.data(), static_cast<uint16_t>(m_Name.size()));
}

DX12CommandQueue::~DX12CommandQueue() {
    WaitIdle();
    TracyD3D12Destroy(m_TracyCtx);
}

void DX12CommandQueue::Submit(std::span<const std::reference_wrapper<const CommandContext>> contexts,
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

    std::pmr::vector<ID3D12CommandList*> command_lists;
    std::transform(
        contexts.begin(), contexts.end(),
        std::back_inserter(command_lists),
        [](const CommandContext& ctx) -> ID3D12CommandList* {
            switch (ctx.GetType()) {
                case CommandType::Graphics:
                    return dynamic_cast<const DX12GraphicsCommandList&>(ctx).command_list.Get();
                case CommandType::Compute:
                    return dynamic_cast<const DX12ComputeCommandList&>(ctx).command_list.Get();
                case CommandType::Copy:
                    return dynamic_cast<const DX12CopyCommandList&>(ctx).command_list.Get();
                default:
                    utils::unreachable();
            }
        });

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
    m_Queue->Signal(m_Fence.GetFence().Get(), ++m_SubmitCount);
}

void DX12CommandQueue::WaitIdle() {
    if (m_SubmitCount == 0) return;
    m_Fence.Wait(m_SubmitCount);
}

void DX12CommandQueue::NewFrame() {
    TracyD3D12NewFrame(m_TracyCtx);
}

}  // namespace hitagi::gfx
