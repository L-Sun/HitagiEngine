#pragma once
#include "dx12_sync.hpp"
#include <hitagi/gfx/command_queue.hpp>

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;

class DX12CommandQueue : public CommandQueue {
public:
    DX12CommandQueue(DX12Device& device, CommandType type, std::string_view name);

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final;

    void WaitIdle() final;

    inline auto GetDX12Queue() const noexcept { return m_Queue; }

private:
    ComPtr<ID3D12CommandQueue> m_Queue;
    DX12Fence                  m_Fence;
    std::uint64_t              m_SubmitCount = 0;
};
}  // namespace hitagi::gfx