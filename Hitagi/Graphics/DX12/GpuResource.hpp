#pragma once

#include "../Resource.hpp"
#include "D3Dpch.hpp"

namespace hitagi::graphics::backend::DX12 {

class GpuResource : public backend::Resource {
    friend class CommandContext;
    friend class GraphicsCommandContext;
    friend class CopyCommandContext;

public:
    GpuResource(ID3D12Resource* resource = nullptr, D3D12_RESOURCE_STATES usage = D3D12_RESOURCE_STATE_COMMON)
        : m_UsageState(usage),
          m_TransitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)) {
        if (resource) m_Resource.Attach(resource);
    }

    ID3D12Resource*       operator->() { return m_Resource.Get(); }
    const ID3D12Resource* operator->() const { return m_Resource.Get(); }

    ID3D12Resource*       GetResource() { return m_Resource.Get(); }
    const ID3D12Resource* GetResource() const { return m_Resource.Get(); }

protected:
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES  m_UsageState;
    D3D12_RESOURCE_STATES  m_TransitioningState;
};
}  // namespace hitagi::graphics::backend::DX12