#pragma once
#include <hitagi/gfx/sync.hpp>

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;

class DX12Fence final : public Fence {
public:
    DX12Fence(DX12Device& device, std::uint64_t initial_value, std::string_view name);
    ~DX12Fence() final;

    void Signal(std::uint64_t value) final;
    bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) final;
    auto GetCurrentValue() -> std::uint64_t final;

    inline auto GetFence() const noexcept { return m_Fence; }

private:
    ComPtr<ID3D12Fence> m_Fence;
    void*               m_EventHandle = nullptr;
};
}  // namespace hitagi::gfx