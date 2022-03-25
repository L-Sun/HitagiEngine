#include "depth_buffer.hpp"

namespace hitagi::graphics::backend::DX12 {

void DepthBuffer::Create(std::wstring_view name, uint32_t width, uint32_t height, DXGI_FORMAT format, unsigned sampleCount, unsigned sampleQuality) {
    auto desc                       = DescribeTex2D(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    desc.SampleDesc.Count           = sampleCount;
    desc.SampleDesc.Quality         = sampleQuality;
    D3D12_CLEAR_VALUE clearValue    = {};
    clearValue.Format               = format;
    clearValue.DepthStencil.Depth   = m_ClearDepth;
    clearValue.DepthStencil.Stencil = m_ClearStencil;
    CreateTextureResource(name, desc, clearValue);
    CreateDerivedViews(format);
}

void DepthBuffer::CreateDerivedViews(DXGI_FORMAT format) {
    ID3D12Resource* resource = m_Resource.Get();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format = GetDSVFormat(format);
    if (resource->GetDesc().SampleDesc.Count == 1) {
        dsvDesc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
    } else {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    }

    if (!m_DSV[0]) {
        m_DSV[0] = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate();
        m_DSV[1] = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate();
    }

    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    g_Device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[0].GetDescriptorCpuHandle());

    dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    g_Device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[1].GetDescriptorCpuHandle());

    DXGI_FORMAT stencilReadFormat = GetStencilFormat(format);
    if (stencilReadFormat != DXGI_FORMAT_UNKNOWN) {
        if (!m_DSV[2]) {
            m_DSV[2] = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate();
            m_DSV[3] = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate();
        }

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        g_Device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[2].GetDescriptorCpuHandle());

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        g_Device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[3].GetDescriptorCpuHandle());
    } else {
        // m_DSV[2] and m_DSV[3] will be replaced with m_DSV[0] and m_DSV[1] respectively
    }

    if (!m_DepthSRV)
        m_DepthSRV = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate();

    // Create the shader resource view
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format                          = GetDepthFormat(format);
    if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D) {
        SRVDesc.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = 1;
    } else {
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    }
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    g_Device->CreateShaderResourceView(resource, &SRVDesc, m_DepthSRV.GetDescriptorCpuHandle());

    if (stencilReadFormat != DXGI_FORMAT_UNKNOWN) {
        if (!m_StencilSRV)
            m_StencilSRV = g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate();

        SRVDesc.Format = stencilReadFormat;
        g_Device->CreateShaderResourceView(resource, &SRVDesc, m_StencilSRV.GetDescriptorCpuHandle());
    }
}

}  // namespace hitagi::graphics::backend::DX12