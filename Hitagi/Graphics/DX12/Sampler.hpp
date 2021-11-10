#pragma once
#include "../Resource.hpp"
#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::backend::DX12 {

class Sampler : public backend::Resource {
public:
    Sampler(ID3D12Device* device, Descriptor&& sampler, const D3D12_SAMPLER_DESC& desc);
    const Descriptor& GetDescriptor() const noexcept { return m_Sampler; }

private:
    Descriptor m_Sampler;
};
}  // namespace Hitagi::Graphics::backend::DX12