module;
#include <d3d12.h>
#include <tracy/TracyD3D12.hpp>
#include <wrl.h>

export module gfx.dx12:command_queue;
import std;
import gfx.base;
import :types;
import :sync;

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;

class DX12CommandQueue : public CommandQueue {
public:
    DX12CommandQueue(DX12Device& device, CommandType type, std::string_view name);
    ~DX12CommandQueue() override;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final;

    void WaitIdle() final;
    void NewFrame();

    inline auto GetDX12Queue() const noexcept { return m_Queue; }
    inline auto GetTracyCtx() const noexcept { return m_TracyCtx; }

private:
    ComPtr<ID3D12CommandQueue> m_Queue;
    DX12Fence                  m_Fence;
    std::uint64_t              m_SubmitCount = 0;
    TracyD3D12Ctx              m_TracyCtx    = nullptr;
};
}  // namespace hitagi::gfx
