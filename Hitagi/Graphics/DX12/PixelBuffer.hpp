#pragma once

#include <filesystem>

#include "GpuResource.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Graphics::DX12 {

class PixelBuffer : public GpuResource {
public:
    PixelBuffer() : m_Width(0), m_Height(0), m_ArraySize(0), m_Format(DXGI_FORMAT_A8_UNORM) {}

    uint32_t           GetWidth() const { return m_Width; }
    uint32_t           GetHeight() const { return m_Height; }
    uint32_t           GetDepth() const { return m_ArraySize; }
    const DXGI_FORMAT& GetFormat() const { return m_Format; }

    // TODO: ReadBackBuffer
    void ExportToFile(const std::filesystem::path& filePath);

protected:
    D3D12_RESOURCE_DESC DescribeTex2D(
        uint32_t             width,
        uint32_t             height,
        uint32_t             depthOrArraySize,
        uint32_t             numMips,
        DXGI_FORMAT          format,
        D3D12_RESOURCE_FLAGS flags);

    void AssociateWithResource(std::wstring_view name, ID3D12Resource* resource, D3D12_RESOURCE_STATES currentState);

    void CreateTextureResource(
        std::wstring_view   name,
        D3D12_RESOURCE_DESC desc,
        D3D12_CLEAR_VALUE   clearValue);

    static DXGI_FORMAT GetBaseFormat(DXGI_FORMAT format);
    static DXGI_FORMAT GetUAVFormat(DXGI_FORMAT format);
    static DXGI_FORMAT GetDSVFormat(DXGI_FORMAT format);
    static DXGI_FORMAT GetDepthFormat(DXGI_FORMAT format);
    static DXGI_FORMAT GetStencilFormat(DXGI_FORMAT format);
    static size_t      BytesPerPixel(DXGI_FORMAT format);

    uint32_t    m_Width;
    uint32_t    m_Height;
    uint32_t    m_ArraySize;
    DXGI_FORMAT m_Format;
};

}  // namespace Hitagi::Graphics::DX12