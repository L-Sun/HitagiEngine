#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {

Microsoft::WRL::ComPtr<ID3D12Device6>                                 g_Device = nullptr;
CommandListManager                                                    g_CommandManager;
std::array<DescriptorAllocator, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> g_DescriptorAllocator = {
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
};
}  // namespace Hitagi::Graphics::DX12