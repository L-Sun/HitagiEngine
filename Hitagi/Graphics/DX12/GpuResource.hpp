#pragma once
#include "D3Dpch.hpp"
#include "../Resource.hpp"

namespace Hitagi::Graphics::backend::DX12 {

class GpuResource : public Resource {
    friend class CommandContext;

public:
    GpuResource(std::string_view name, ID3D12Resource* resource = nullptr)
        : Resource(name), m_TransitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)) {
        if (resource) m_Resource.Attach(resource);
    }

    ID3D12Resource*       operator->() { return m_Resource.Get(); }
    const ID3D12Resource* operator->() const { return m_Resource.Get(); }

    ID3D12Resource*       GetResource() { return m_Resource.Get(); }
    const ID3D12Resource* GetResource() const { return m_Resource.Get(); }

protected:
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES  m_UsageState{D3D12_RESOURCE_STATE_COMMON};
    D3D12_RESOURCE_STATES  m_TransitioningState;
};
}  // namespace Hitagi::Graphics::backend::DX12