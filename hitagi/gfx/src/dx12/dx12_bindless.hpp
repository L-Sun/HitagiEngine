#pragma once
#include "dx12_descriptor_heap.hpp"
#include <hitagi/gfx/bindless.hpp>

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;
class DX12BindlessUtils : public BindlessUtils {
public:
    DX12BindlessUtils(DX12Device& device, std::string_view name);

    auto CreateBindlessHandle(GPUBuffer& buffer, std::size_t index = 0, bool writable = false) -> BindlessHandle final;
    auto CreateBindlessHandle(Texture& texture, bool writeable = false) -> BindlessHandle final;
    auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle final;

    void DiscardBindlessHandle(BindlessHandle handle) final;

    inline auto GetDescriptorHeaps() const noexcept -> std::array<ID3D12DescriptorHeap*, 2> {
        return {
            m_CBV_SRV_UAV_DescriptorHeap.GetHeap(),
            m_Sampler_DescriptorHeap.GetHeap(),
        };
    }
    inline auto GetBindlessRootSignature() const noexcept { return m_RootSignature; }

private:
    std::mutex m_Mutex;

    ComPtr<ID3D12RootSignature> m_RootSignature;

    DescriptorHeap m_CBV_SRV_UAV_DescriptorHeap;
    DescriptorHeap m_Sampler_DescriptorHeap;
    Descriptor     m_CBV_SRV_UAV_Descriptors;
    Descriptor     m_Sampler_Descriptors;

    std::pmr::vector<BindlessHandle> m_Available_CBV_SRV_UAV_BindlessHandles;
    std::pmr::vector<BindlessHandle> m_Available_Sampler_BindlessHandles;
};
}  // namespace hitagi::gfx