#pragma once
#include <hitagi/gfx/bindless.hpp>

#include <d3d12.h>
#include <wrl.h>

#include <deque>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;
class DX12BindlessUtils : public BindlessUtils {
public:
    DX12BindlessUtils(DX12Device& device, std::string_view name);

    auto CreateBindlessHandle(GPUBuffer& buffer, bool writable = false) -> BindlessHandle final;
    auto CreateBindlessHandle(Texture& texture, bool writeable = false) -> BindlessHandle final;
    auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle final;

    void DiscardBindlessHandle(BindlessHandle handle) final;

    inline auto GetDescriptorHeaps() const noexcept -> std::array<ID3D12DescriptorHeap*, 2> {
        return {
            m_CBV_SRV_UAV_DescriptorHeap.Get(),
            m_Sampler_DescriptorHeap.Get(),
        };
    }
    inline auto GetBindlessRootSignature() const noexcept { return m_RootSignature; }

private:
    std::mutex m_Mutex;

    ComPtr<ID3D12RootSignature> m_RootSignature;

    ComPtr<ID3D12DescriptorHeap> m_CBV_SRV_UAV_DescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> m_Sampler_DescriptorHeap;

    std::pmr::deque<BindlessHandle> m_Available_CBV_SRV_UAV_BindlessHandlePool;
    std::pmr::deque<BindlessHandle> m_Available_Sampler_BindlessHandlePool;
};
}  // namespace hitagi::gfx