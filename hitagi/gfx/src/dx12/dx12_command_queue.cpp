#include "dx12_command_queue.hpp"
#include "dx12_device.hpp"
#include "dx12_command_context.hpp"
#include "utils.hpp"

#include <d3d12.h>
#include <fmt/color.h>

namespace hitagi::gfx {
DX12CommandQueue::DX12CommandQueue(DX12Device& device, CommandType type, std::string_view name)
    : CommandQueue(device, type, name) {
    D3D12_COMMAND_QUEUE_DESC desc{
        .Type     = to_d3d_command_type(type),
        .Priority = 0,
        .Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    ThrowIfFailed(device.GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue)));
    ThrowIfFailed(device.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));

    m_Fence->Signal(m_LastCompletedFenceValue);

    if (m_FenceHandle = CreateEvent(nullptr, false, false, nullptr);
        m_FenceHandle == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (!name.empty()) {
        m_Queue->SetName(std::wstring(name.begin(), name.end()).data());
        m_Fence->SetName(std::wstring(name.begin(), name.end()).data());
    }
}

DX12CommandQueue::~DX12CommandQueue() {
    WaitIdle();
    CloseHandle(m_FenceHandle);
}

auto DX12CommandQueue::Submit(std::pmr::vector<CommandContext*> contexts) -> std::uint64_t {
    if (auto iter = std::find_if(contexts.cbegin(), contexts.cend(), [this](const auto& cmd) -> bool {
            return cmd->type != type;
        });
        iter != contexts.cend()) {
        device.GetLogger()->error(
            "Can not submit {} command to this {} command queue",
            fmt::styled(magic_enum::enum_name((*iter)->type), fmt::fg(fmt::color::red)),
            fmt::styled(magic_enum::enum_name(type), fmt::fg(fmt::color::green)));

        throw std::invalid_argument(fmt::format(
            "Can not submit {} command to this {} command queue",
            magic_enum::enum_name((*iter)->type),
            magic_enum::enum_name(type)));

        return 0;
    }

    std::pmr::vector<ID3D12CommandList*> d3d_cmd_lists;
    for (const auto& context : contexts) {
        context->fence_value = m_NextFenceValue;
        d3d_cmd_lists.emplace_back(dynamic_cast<DX12CommandContext*>(context)->m_CmdList.Get());
    }

    m_Queue->ExecuteCommandLists(d3d_cmd_lists.size(), d3d_cmd_lists.data());
    // When queue finished all command list, it will set a new fence value `m_NextFenceValue`
    return InsertFence();
}

bool DX12CommandQueue::IsFenceComplete(std::uint64_t fence_value) {
    if (fence_value > m_LastCompletedFenceValue) {
        m_LastCompletedFenceValue = m_Fence->GetCompletedValue();
    }
    return fence_value <= m_LastCompletedFenceValue;
}

void DX12CommandQueue::WaitForFence(std::uint64_t fence_value) {
    if (IsFenceComplete(fence_value)) return;

    std::lock_guard<std::mutex> lock{m_EventMutex};

    ThrowIfFailed(m_Fence->SetEventOnCompletion(fence_value, m_FenceHandle));
    WaitForSingleObject(m_FenceHandle, INFINITE);
    m_LastCompletedFenceValue = fence_value;
}

void DX12CommandQueue::WaitForQueue(const CommandQueue& other) {
    auto& _other = static_cast<const DX12CommandQueue&>(other);
    assert(_other.m_NextFenceValue > 0);
    m_Queue->Wait(_other.m_Fence.Get(), _other.m_NextFenceValue - 1);
}

void DX12CommandQueue::WaitIdle() {
    WaitForFence(m_NextFenceValue - 1);
}

std::uint64_t DX12CommandQueue::InsertFence() {
    std::lock_guard lock{m_FenceMutex};

    ThrowIfFailed(m_Queue->Signal(m_Fence.Get(), m_NextFenceValue));
    return m_NextFenceValue++;
}

}  // namespace hitagi::gfx