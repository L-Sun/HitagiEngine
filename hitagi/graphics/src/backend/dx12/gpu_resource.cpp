#include "gpu_resource.hpp"
#include "dx12_device.hpp"

namespace hitagi::graphics::backend::DX12 {
GpuResource::GpuResource(DX12Device* device, ID3D12Resource* resource)
    : m_Device(device),
      m_TransitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)) {
    if (resource) m_Resource.Attach(resource);
}
}  // namespace hitagi::graphics::backend::DX12