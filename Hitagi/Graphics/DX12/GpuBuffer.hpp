#pragma once
#include "Image.hpp"
#include "GpuResource.hpp"

namespace Hitagi::Graphics::DX12 {
class GpuBuffer : public GpuResource {
public:
    GpuBuffer(ID3D12Device6* device, size_t numElement, size_t elementSize, const void* initialData = nullptr,
              CommandContext* context = nullptr);
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

protected:
    ID3D12Device6*       m_Device;
    size_t               m_ElementCount;
    size_t               m_ElementSize;
    size_t               m_BufferSize;
    D3D12_RESOURCE_FLAGS m_ResourceFlags;
};

class TextureBuffer : public GpuResource {
public:
    TextureBuffer(CommandContext& context, D3D12_CPU_DESCRIPTOR_HANDLE handle, DXGI_SAMPLE_DESC sampleDesc,
                  const Resource::Image& image);

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_CpuDescriptorHandle; }
    bool                               operator!() { return m_CpuDescriptorHandle.ptr == 0; }

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuDescriptorHandle;
};

}  // namespace Hitagi::Graphics::DX12