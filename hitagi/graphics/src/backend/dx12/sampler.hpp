#pragma once
#include "descriptor_allocator.hpp"

#include <hitagi/graphics/gpu_resource.hpp>

namespace hitagi::graphics::backend::DX12 {
class DX12Device;
class Sampler : public backend::Resource {
public:
    Sampler(DX12Device* device, const D3D12_SAMPLER_DESC& desc);
    const auto& GetDescriptor() const noexcept { return m_Sampler; }

private:
    DX12Device*                 m_Device = nullptr;
    std::shared_ptr<Descriptor> m_Sampler;
};
}  // namespace hitagi::graphics::backend::DX12