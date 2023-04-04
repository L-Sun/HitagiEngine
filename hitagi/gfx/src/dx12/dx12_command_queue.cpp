#include "dx12_command_queue.hpp"
#include "dx12_command_list.hpp"
#include "dx12_sync.hpp"
#include "dx12_device.hpp"
#include "dx12_utils.hpp"

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {
DX12CommandQueue::DX12CommandQueue(DX12Device& device, CommandType type, std::string_view name)
    : CommandQueue(device, type, name),
      m_Fence(device, 0, fmt::format("{}_Fence", name)) {
    const auto logger = device.GetLogger();

    const D3D12_COMMAND_QUEUE_DESC desc{
        .Type     = to_d3d_command_type(type),
        .Priority = 0,
        .Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };

    logger->trace("Creating Command Queue: {}", fmt::styled(name, fmt::fg(fmt::color::green)));
    if (FAILED(device.GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue)))) {
        const auto error_message = fmt::format("Failed to create Command Queue({})", fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    m_Queue->SetName(std::wstring(name.begin(), name.end()).c_str());
}

void DX12CommandQueue::Submit(const std::pmr::vector<CommandContext*>& contexts,
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

    std::pmr::vector<ID3D12CommandList*> command_lists;
    std::transform(
        contexts.begin(), contexts.end(),
        std::back_inserter(command_lists),
        [](auto ctx) -> ID3D12CommandList* {
            switch (ctx->GetType()) {
                case CommandType::Graphics:
                    return static_cast<DX12GraphicsCommandList*>(ctx)->command_list.Get();
                case CommandType::Compute:
                    return static_cast<DX12ComputeCommandList*>(ctx)->command_list.Get();
                case CommandType::Copy:
                    return static_cast<DX12CopyCommandList*>(ctx)->command_list.Get();
            }
        });

    for (const auto& wait_fence : wait_fences) {
        const auto& fence = static_cast<DX12Fence&>(wait_fence.fence);
        m_Queue->Wait(fence.GetFence().Get(), wait_fence.value);
    }

    if (!command_lists.empty()) {
        m_Queue->ExecuteCommandLists(command_lists.size(), command_lists.data());
    }

    for (const auto& signal_fence : signal_fences) {
        const auto& fence = static_cast<DX12Fence&>(signal_fence.fence);
        m_Queue->Signal(fence.GetFence().Get(), signal_fence.value);
    }
    m_Queue->Signal(m_Fence.GetFence().Get(), ++m_SubmitCount);
}

void DX12CommandQueue::WaitIdle() {
    m_Fence.Wait(m_SubmitCount);
}

}  // namespace hitagi::gfx