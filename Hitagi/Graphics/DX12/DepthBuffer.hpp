#pragma once

#include "PixelBuffer.hpp"
#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::DX12 {

class DepthBuffer : public PixelBuffer {
public:
    DepthBuffer(float clearDepth = 1.0f, uint8_t clearStencil = 0)
        : m_ClearDepth(clearDepth), m_ClearStencil(clearStencil) {}

    void Create(std::wstring_view name, uint32_t width, uint32_t height,DXGI_FORMAT format, unsigned sampleCount = 1, unsigned sampleQuality = 0);

    // Get pre-created CPU-visible descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DSV[0].GetDescriptorCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV_DepthReadOnly() const { return m_DSV[1].GetDescriptorCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV_StencilReadOnly() const { return m_DSV[2] ? m_DSV[2].GetDescriptorCpuHandle() : GetDSV(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV_ReadOnly() const { return m_DSV[3] ? m_DSV[3].GetDescriptorCpuHandle() : GetDSV_DepthReadOnly(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthSRV() const { return m_DepthSRV.GetDescriptorCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetStencilSRV() const { return m_StencilSRV.GetDescriptorCpuHandle(); }

    float   GetClearDepth() const { return m_ClearDepth; }
    uint8_t GetClearStencil() const { return m_ClearStencil; }

private:
    void CreateDerivedViews(DXGI_FORMAT format);

    float   m_ClearDepth;
    uint8_t m_ClearStencil;

    std::array<DescriptorAllocation, 4> m_DSV;
    DescriptorAllocation                m_DepthSRV;
    DescriptorAllocation                m_StencilSRV;
};

}  // namespace Hitagi::Graphics::DX12