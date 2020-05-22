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
    uint32_t          numMipMaps,
    const vec4f&      clearColor) {
    m_ClearColor = clearColor;
    m_NumMipMaps = numMipMaps == 0 ? ComputeNumMips(width, height) : numMipMaps;

    auto desc               = DescribeTex2D(width, height, 1, m_NumMipMaps, format, CombineResourceFlags());
    desc.SampleDesc.Count   = m_SampleCount;
    desc.SampleDesc.Quality = m_SampleQuality;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format            = format;
    clearValue.Color[0]          = clearColor.r;
    clearValue.Color[1]          = clearColor.g;
    clearValue.Color[2]          = clearColor.b;
    clearValue.Color[3]          = clearColor.a;

    CreateTextureResource(name, desc, clearValue);
    CreateDerivedViews(format, 1);
}

void ColorBuffer::GenerateMipMaps(CommandContext& context) {
}

void ColorBuffer::CreateDerivedViews(DXGI_FORMAT format, uint32_t arraySize) {
    assert((arraySize == 1 || m_NumMipMaps == 1) && "We don't support auto-mips on texture arrays");

    D3D12_RENDER_TARGET_VIEW_DESC    RTVDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC  SRVDesc = {};

    RTVDesc.Format                  = format;
    UAVDesc.Format                  = GetUAVFormat(format);
    SRVDesc.Format                  = format;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (arraySize > 1) {
        RTVDesc.ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        RTVDesc.Texture2DArray.MipSlice        = 0;
        RTVDesc.Texture2DArray.FirstArraySlice = 0;
        RTVDesc.Texture2DArray.ArraySize       = arraySize;

        UAVDesc.ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        UAVDesc.Texture2DArray.MipSlice        = 0;
        UAVDesc.Texture2DArray.FirstArraySlice = 0;
        UAVDesc.Texture2DArray.ArraySize       = arraySize;

        SRVDesc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        SRVDesc.Texture2DArray.MipLevels       = m_NumMipMaps;
        SRVDesc.Texture2DArray.MostDetailedMip = 0;
        SRVDesc.Texture2DArray.FirstArraySlice = 0;
        SRVDesc.Texture2DArray.ArraySize       = arraySize;
    } else if (m_SampleCount > 1) {
        RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;

    } else {
        RTVDesc.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
        RTVDesc.Texture2D.MipSlice = 0;

        UAVDesc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
        UAVDesc.Texture2D.MipSlice = 0;

        SRVDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels       = m_NumMipMaps;
        SRVDesc.Texture2D.MostDetailedMip = 0;
    }

    if (!m_SRVHandle) {
        m_RTVHandle = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate();
        m_SRVHandle = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate();
    }

    ID3D12Resource* Resource = m_Resource.Get();

    // Create the render target view
    g_Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle.GetDescriptorCpuHandle());

    // Create the shader resource view
    g_Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle.GetDescriptorCpuHandle());

    if (m_SampleCount > 1)
        return;

    // Create the UAVs for each mip level (RWTexture2D)
    for (uint32_t i = 0; i < m_NumMipMaps; ++i) {
        if (!m_UAVHandle[i])
            m_UAVHandle[i] = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate();

        g_Device->CreateUnorderedAccessView(Resource, nullptr, &UAVDesc, m_UAVHandle[i].GetDescriptorCpuHandle());

        UAVDesc.Texture2D.MipSlice++;
    }
}

}  // namespace Hitagi::Graphics::DX12