#pragma once
#include "Image.hpp"
#include "GpuResource.hpp"
#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::DX12 {
class GpuBuffer : public GpuResource {
public:
    GpuBuffer() = default;
    GpuBuffer(std::wstring_view name, size_t numElement, size_t elementSize, const void* initialData = nullptr) {
        Create(name, numElement, elementSize, initialData);
    }
    void Create(std::wstring_view name, size_t numElement, size_t elementSize, const void* initialData = nullptr);

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = m_Resource->GetGPUVirtualAddress();
        vbv.SizeInBytes    = m_BufferSize;
        vbv.StrideInBytes  = m_ElementSize;
        return vbv;
    }
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = m_Resource->GetGPUVirtualAddress();
        ibv.Format         = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes    = m_BufferSize;
        return ibv;
    }
    size_t GetBufferSize() const { return m_BufferSize; }

protected:
    size_t               m_ElementCount;
    size_t               m_ElementSize;
    size_t               m_BufferSize;
    D3D12_RESOURCE_FLAGS m_ResourceFlags;
};

class TextureBuffer : public GpuResource {
public:
    TextureBuffer(std::wstring_view name, const Resource::Image& image) { Create(name, image); }
    void Create(std::wstring_view name, const Resource::Image& image);

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_SRV.GetDescriptorCpuHandle(); }
    operator bool() const noexcept { return static_cast<bool>(m_SRV); }

private:
    DescriptorAllocation m_SRV;
};

}  // namespace Hitagi::Graphics::DX12