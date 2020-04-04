#pragma once
#include "D3Dpch.hpp"

namespace Hitagi::Graphics {
class GpuResource {
    friend class CommandContext;

public:
    GpuResource()
        : m_Resource(nullptr),
          m_UsageState(D3D12_RESOURCE_STATE_COMMON),
          m_TransitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)),
          m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL) {}
    ~GpuResource() { m_GpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL; }
    ID3D12Resource*       operator->() { return m_Resource.Get(); }
    const ID3D12Resource* operator->() const { return m_Resource.Get(); }

    ID3D12Resource*       GetResource() { return m_Resource.Get(); }
    const ID3D12Resource* GetResource() const { return m_Resource.Get(); }

protected:
    void UpdateGpuVirtualAddress() { m_GpuVirtualAddress = m_Resource->GetGPUVirtualAddress(); }

    Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES                  m_UsageState;
    D3D12_RESOURCE_STATES                  m_TransitioningState;
    D3D12_GPU_VIRTUAL_ADDRESS              m_GpuVirtualAddress;
};
}  // namespace Hitagi::Graphics