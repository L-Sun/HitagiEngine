#pragma once
#include <hitagi/gfx/command_queue.hpp>

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;

class DX12CommandQueue final : public CommandQueue {
public:
    DX12CommandQueue(DX12Device* device, CommandType type, std::string_view name);
    ~DX12CommandQueue() final;

    auto Submit(std::pmr::vector<CommandContext*> context) -> std::uint64_t final;
    bool IsFenceComplete(std::uint64_t fence_value) const final;
    void WaitForFence(std::uint64_t fence_value) final;
    void WaitForQueue(const CommandQueue& other) final;
    void WaitIdle() final;

    inline ID3D12CommandQueue* GetD3DCommandQueue() const noexcept { return m_Queue.Get(); }
    std::uint64_t              InsertFence();

private:
    ComPtr<ID3D12CommandQueue>   m_Queue;
    ComPtr<ID3D12Fence>          m_Fence;
    HANDLE                       m_FenceHandle;
    std::atomic_uint64_t         m_NextFenceValue          = 1;
    mutable std::atomic_uint64_t m_LastCompletedFenceValue = 0;
};
}  // namespace hitagi::gfx