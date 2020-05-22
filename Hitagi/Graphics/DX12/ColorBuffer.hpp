#pragma once

#include "PixelBuffer.hpp"
#include "DescriptorAllocator.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Graphics::DX12 {

class ColorBuffer : public PixelBuffer {
public:
    // Create from swap chain
    void CreateFromSwapChain(std::wstring_view name, ID3D12Resource* resource);

    void Create(
        std::wstring_view name,
        uint32_t          width,
        uint32_t          height,
        DXGI_FORMAT       format,
        uint32_t          numMipMaps = 0,
        const vec4f&      clearColor = vec4f(0, 0, 0, 0));

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_SRVHandle.GetDescriptorCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_RTVHandle.GetDescriptorCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return m_UAVHandle[0].GetDescriptorCpuHandle(); }

    void         SetClearColor(const vec4f& clearColor) { m_ClearColor = clearColor; }
    const vec4f& GetClearColor() const { return m_ClearColor; }

    void SetMsaaMode(unsigned MsaaCount, unsigned MsaaQuality) {
        m_SampleCount   = MsaaCount;
        m_SampleQuality = MsaaQuality;
    }

    // TODO generate mipMaps
    void GenerateMipMaps(CommandContext& context);

protected:
    void CreateDerivedViews(DXGI_FORMAT format, uint32_t arraySize);

    static inline uint32_t ComputeNumMips(uint32_t width, uint32_t height) {
        uint32_t highBit;
        _BitScanReverse((unsigned long*)&highBit, width | height);
        return highBit + 1;
    }
    D3D12_RESOURCE_FLAGS CombineResourceFlags() {
        D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
        return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
    }
    vec4f                                m_ClearColor;
    DescriptorAllocation                 m_SRVHandle;
    DescriptorAllocation                 m_RTVHandle;
    std::array<DescriptorAllocation, 12> m_UAVHandle;

    uint32_t m_NumMipMaps;  // number of texture sublevels
    unsigned m_SampleCount   = 1;
    unsigned m_SampleQuality = 0;
};

}  // namespace Hitagi::Graphics::DX12