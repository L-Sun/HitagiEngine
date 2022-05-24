#pragma once
#include "gpu_resource.hpp"
#include "descriptor_allocator.hpp"
#include "utils.hpp"

namespace hitagi::graphics::backend::DX12 {

class GpuBuffer : public GpuResource {
public:
    GpuBuffer(ID3D12Device* device, std::string_view name, std::size_t size, D3D12_RESOURCE_STATES usage = D3D12_RESOURCE_STATE_COMMON);
    std::size_t GetBufferSize() const { return m_BufferSize; }

protected:
    std::size_t          m_BufferSize    = 0;
    D3D12_RESOURCE_FLAGS m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
};

class VertexBuffer : public GpuBuffer {
public:
    VertexBuffer(ID3D12Device* device, std::string_view name, graphics::VertexBufferDesc desc);

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView(std::size_t slot) const;
    std::size_t              GetVertexCount() const { return m_Desc.vertex_count; }

    bool        SlotEnabled(std::size_t slot) const;
    std::size_t GetSlotOffset(std::size_t slot) const;
    std::size_t GetSlotSize(std::size_t slot) const;
    std::size_t GetSlotElementSize(std::size_t slot) const;

private:
    graphics::VertexBufferDesc m_Desc;
};

class IndexBuffer : public GpuBuffer {
public:
    IndexBuffer(ID3D12Device* device, std::string_view name, graphics::IndexBufferDesc desc);

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = m_Resource->GetGPUVirtualAddress();
        ibv.Format         = index_size_to_dxgi_format(m_Desc.index_size);
        ibv.SizeInBytes    = m_BufferSize;
        return ibv;
    }

private:
    graphics::IndexBufferDesc m_Desc;
};

class ConstantBuffer : public GpuResource {
public:
    ConstantBuffer(std::string_view name, ID3D12Device* device, DescriptorAllocator& descritptor_allocator, graphics::ConstantBufferDesc desc);
    ~ConstantBuffer() override;
    void                     UpdateData(size_t index, const std::byte* data, size_t data_size);
    void                     Resize(ID3D12Device* device, DescriptorAllocator& descritptor_allocator, size_t new_num_elements);
    inline const Descriptor& GetCBV(size_t index) const { return m_CBV.at(index); }

private:
    std::byte*                   m_CpuPtr = nullptr;
    graphics::ConstantBufferDesc m_Desc;

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

}  // namespace hitagi::graphics::backend::DX12