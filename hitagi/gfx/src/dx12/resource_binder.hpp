#pragma once
#include "descriptor_heap.hpp"
#include <hitagi/gfx/gpu_resource.hpp>

#include <d3d12.h>
#include <wrl.h>
#include <bitset>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12CommandContext;

class ResourceBinder {
public:
    enum struct SlotType : std::uint8_t {
        CBV,
        SRV,
        UAV,
        Sampler
    };

    ResourceBinder(DX12CommandContext& context);
    ~ResourceBinder();

    void SetRootSignature(const D3D12_ROOT_SIGNATURE_DESC1* root_sig_desc);

    void PushConstant(std::uint32_t slot, const std::span<const std::byte>& data);
    void BindConstantBuffer(std::uint32_t slot, const GpuBufferView& buffer, std::size_t index = 0);
    void BindResource(std::uint32_t slot, const TextureView& resource);

    // Commit all bind cpu descriptor to gpu
    void FlushDescriptors();

private:
    constexpr static std::uint8_t sm_MaxRootParameters = 32;
    constexpr static std::size_t  sm_HeapSize          = 1024;

    enum struct BindingType {
        RootConstant,
        RootDescriptor,
        DescriptorTable,
    };
    struct SlotInfo {
        BindingType   binding_type;
        std::uint32_t param_index;
        // this will used if `binding_type == DescriptorTable`
        std::size_t offset_in_heap = 0;
    };

    void CacheDescriptor(SlotType slot_type, const Descriptor& descriptor, std::size_t descriptor_index, std::size_t heap_offset);

    // A Reference to command context
    DX12CommandContext&               m_Context;
    const D3D12_ROOT_SIGNATURE_DESC1* m_CurrRootSignatureDesc = nullptr;

    utils::EnumArray<std::pmr::unordered_map<std::uint32_t, SlotInfo>, SlotType> m_SlotInfos;

    std::pmr::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_CBV_UAV_SRV_Cache;
    std::pmr::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_Sampler_Cache;

    std::array<std::size_t, sm_MaxRootParameters> m_TableHeapOffset;
    std::bitset<sm_MaxRootParameters>             m_TableMask;

    std::pmr::vector<Descriptor> m_UsedHeaps;
};
}  // namespace hitagi::gfx