#pragma once
#include "descriptor_allocator.hpp"

#include <hitagi/graphics/resource.hpp>

namespace hitagi::graphics::backend::DX12 {

class Sampler : public backend::Resource {
public:
    Sampler(ID3D12Device* device, std::shared_ptr<Descriptor> sampler, const D3D12_SAMPLER_DESC& desc);
    const auto& GetDescriptor() const noexcept { return m_Sampler; }

private:
    std::shared_ptr<Descriptor> m_Sampler;
};
}  // namespace hitagi::graphics::backend::DX12