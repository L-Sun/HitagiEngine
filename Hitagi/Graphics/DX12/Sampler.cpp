#include "Sampler.hpp"

namespace Hitagi::Graphics::backend::DX12 {
Sampler::Sampler(ID3D12Device* device, Descriptor&& sampler, const D3D12_SAMPLER_DESC& desc)
    : m_Sampler(std::move(sampler)) {
    device->CreateSampler(&desc, m_Sampler.handle);
}

// ! Why the destructor does not exsist?

}  // namespace Hitagi::Graphics::backend::DX12