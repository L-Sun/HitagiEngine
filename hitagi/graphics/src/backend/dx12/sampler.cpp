#include "sampler.hpp"
#include "dx12_device.hpp"

#include <d3d12.h>

namespace hitagi::gfx::backend::DX12 {
Sampler::Sampler(DX12Device* device, const D3D12_SAMPLER_DESC& desc)
    : m_Device(device), m_Sampler(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).Allocate(Descriptor::Type::Sampler)) {
    device->GetDevice()->CreateSampler(&desc, m_Sampler.handle);
}

}  // namespace hitagi::gfx::backend::DX12