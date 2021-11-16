#pragma once
#include "GpuResource.hpp"
#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::backend::DX12 {

class GpuBuffer : public GpuResource {
public:
    GpuBuffer(ID3D12Device* device, std::string_view name, size_t num_elements, size_t element_size);
    size_t GetBufferSize() const { return m_BufferSize; }
    size_t GetElementCount() const { return m_NumElements; }

protected:
    size_t               m_NumElements;
    size_t               m_ElementSize;
    size_t               m_BufferSize;
    D3D12_RESOURCE_FLAGS m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
};

class VertexBuffer : public GpuBuffer {
public:
    using GpuBuffer::GpuBuffer;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = m_Resource->GetGPUVirtualAddress();
        vbv.SizeInBytes    = m_BufferSize;
        vbv.StrideInBytes  = m_ElementSize;
        return vbv;
    }
};

class IndexBuffer : public GpuBuffer {
public:
    using GpuBuffer::GpuBuffer;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = m_Resource->GetGPUVirtualAddress();
        ibv.Format         = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes    = m_BufferSize;
        return ibv;
    }
};

class ConstantBuffer : public GpuResource {
public:
    ConstantBuffer(std::string_view name, ID3D12Device* device, DescriptorAllocator& descritptor_allocator, size_t num_elements, size_t element_size);
    ~ConstantBuffer() override;
    void              UpdateData(size_t offset, const uint8_t* data, size_t data_size);
    const Descriptor& GetCBV(size_t offset) const { return m_CBV.at(offset); }

private:
    uint8_t*                m_CpuPtr = nullptr;
    size_t                  m_NumElements;
    size_t                  m_ElementSize;
    size_t                  m_BlockSize;  // align(dataSize, 256B)
    size_t                  m_BufferSize;
    std::vector<Descriptor> m_CBV;
};

class TextureBuffer : public GpuResource {
public:
    TextureBuffer(std::string_view name, ID3D12Device* device, Descriptor&& srv, const D3D12_RESOURCE_DESC& desc);
    const Descriptor& GetSRV() const noexcept { return m_SRV; }

private:
    Descriptor m_SRV;
};

class RenderTarget : public GpuResource {
public:
    RenderTarget(std::string_view name, ID3D12Device* device, Descriptor&& rtv, const D3D12_RESOURCE_DESC& desc);
    // Create from exsited resource
    RenderTarget(std::string_view name, ID3D12Device* device, Descriptor&& rtv, ID3D12Resource* res);
    const Descriptor& GetRTV() const noexcept { return m_RTV; }

private:
    Descriptor m_RTV;
};

class DepthBuffer : public GpuResource {
public:
    DepthBuffer(std::string_view name, ID3D12Device* device, Descriptor&& dsv, const D3D12_RESOURCE_DESC& desc, float clear_depth, uint8_t clear_stencil);
    const Descriptor& GetDSV() const noexcept { return m_DSV; }

    float   GetClearDepth() const noexcept { return m_ClearDepth; }
    uint8_t GetClearStencil() const noexcept { return m_ClearStencil; }

private:
    Descriptor m_DSV;
    float      m_ClearDepth;
    uint8_t    m_ClearStencil;
};

}  // namespace Hitagi::Graphics::backend::DX12