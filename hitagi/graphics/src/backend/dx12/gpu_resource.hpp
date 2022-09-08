#pragma once
#include "d3d_pch.hpp"

#include <hitagi/graphics/gpu_resource.hpp>

namespace hitagi::gfx::backend::DX12 {
class DX12Device;

class GpuResource : public backend::Resource {
    friend class CommandContext;
    friend class GraphicsCommandContext;
    friend class CopyCommandContext;

public:
    GpuResource(DX12Device* device, ID3D12Resource* resource = nullptr);

    ID3D12Resource*       operator->() { return m_Resource.Get(); }
    const ID3D12Resource* operator->() const { return m_Resource.Get(); }

    ID3D12Resource*       GetResource() { return m_Resource.Get(); }
    const ID3D12Resource* GetResource() const { return m_Resource.Get(); }

protected:
    DX12Device*            m_Device;
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES  m_ResourceState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES  m_TransitioningState;
};
}  // namespace hitagi::gfx::backend::DX12