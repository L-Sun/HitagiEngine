#include "Sampler.hpp"

namespace Hitagi::Graphics::backend::DX12 {
Sampler::Sampler(std::string_view name, ID3D12Device* device, Descriptor&& sampler, const D3D12_SAMPLER_DESC& desc)
    : Resource(name), m_Sampler(std::move(sampler)) {
    device->CreateSampler(&desc, m_Sampler.handle);
}

}  // namespace Hitagi::Graphics::backend::DX12