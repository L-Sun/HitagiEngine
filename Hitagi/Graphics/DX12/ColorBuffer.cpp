#include "ColorBuffer.hpp"

#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {

// Create from swap chain
void ColorBuffer::CreateFromSwapChain(std::wstring_view name, ID3D12Resource* resource) {
    AssociateWithResource(name, resource, D3D12_RESOURCE_STATE_PRESENT);

    m_RTVHandle = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate();
    g_Device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTVHandle.GetDescriptorCpuHandle());
}

void ColorBuffer::Create(
    std::wstring_view name,
    uint32_t          width,
    uint32_t          height,
    DXGI_FORMAT       format,
    const vec4f&      clearColor) {
    m_ClearColor = clearColor;
    m_NumMipMaps = ComputeNumMips(width, height);

    auto desc = DescribeTex2D(width, height, 1, m_NumMipMaps, format, CombineResourceFlags());

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format            = format;
    clearValue.Color[0]          = clearColor.r;
    clearValue.Color[1]          = clearColor.g;
    clearValue.Color[2]          = clearColor.b;
    clearValue.Color[3]          = clearColor.a;

    CreateTextureResource(name, desc, clearValue);

    D3D12_RENDER_TARGET_VIEW_DESC    RTVDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC  SRVDesc = {};

    RTVDesc.Format                  = format;
    UAVDesc.Format                  = GetUAVFormat(format);
    SRVDesc.Format                  = format;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    RTVDesc.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2D.MipSlice = 0;

    UAVDesc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
    UAVDesc.Texture2D.MipSlice = 0;

    SRVDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels       = m_NumMipMaps;
    SRVDesc.Texture2D.MostDetailedMip = 0;

    if (!m_SRVHandle) {
        m_RTVHandle = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate();
        m_SRVHandle = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate();
    }

    g_Device->CreateRenderTargetView(m_Resource.Get(), &RTVDesc, m_RTVHandle.GetDescriptorCpuHandle());
    g_Device->CreateShaderResourceView(m_Resource.Get(), &SRVDesc, m_SRVHandle.GetDescriptorCpuHandle());

    // Create the UAVs for each mip leve (RWTexture2D)
    for (uint32_t i = 0; i < m_NumMipMaps; i++) {
        if (!m_UAVHandle[i])
            m_UAVHandle[i] = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate();

        g_Device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, &UAVDesc, m_UAVHandle[i].GetDescriptorCpuHandle());

        UAVDesc.Texture2D.MipSlice++;
    }
}

void ColorBuffer::GenerateMipMaps(CommandContext& context) {
}

}  // namespace Hitagi::Graphics::DX12