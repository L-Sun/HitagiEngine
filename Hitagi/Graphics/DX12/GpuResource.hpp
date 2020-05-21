#pragma once
#include "D3Dpch.hpp"

namespace Hitagi::Graphics::DX12 {
class GpuResource {
    friend class CommandContext;

public:
    GpuResource()
        : m_Resource(nullptr),

          m_TransitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)) {}
    ~GpuResource() = default;
    ID3D12Resource*       operator->() { return m_Resource.Get(); }
    const ID3D12Resource* operator->() const { return m_Resource.Get(); }

    ID3D12Resource*       GetResource() { return m_Resource.Get(); }
    const ID3D12Resource* GetResource() const { return m_Resource.Get(); }

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES                  m_UsageState{D3D12_RESOURCE_STATE_COMMON};
    D3D12_RESOURCE_STATES                  m_TransitioningState;
};
}  // namespace Hitagi::Graphics::DX12