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
    VertexBuffer(ID3D12Device* device, graphics::VertexBuffer& vb);

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView(resource::VertexAttribute attr) const;

    bool        AttributeEnabled(resource::VertexAttribute attr) const;
    std::size_t GetAttributeOffset(resource::VertexAttribute attr) const;
    std::size_t GetAttributeSize(resource::VertexAttribute attr) const;
    std::size_t GetAttributeElementSize(resource::VertexAttribute attr) const;
    const auto& GetDesc() const noexcept { return m_Desc; }

private:
    graphics::VertexBufferDesc& m_Desc;
};

class IndexBuffer : public GpuBuffer {
public:
    IndexBuffer(ID3D12Device* device, graphics::IndexBuffer& ib);

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = m_Resource->GetGPUVirtualAddress();
        ibv.Format         = index_size_to_dxgi_format(m_Desc.index_size);
        ibv.SizeInBytes    = m_BufferSize;
        return ibv;
    }
    const auto& GetDesc() const noexcept { return m_Desc; }

private:
    graphics::IndexBufferDesc& m_Desc;
};

class ConstantBuffer : public GpuResource {
public:
    ConstantBuffer(ID3D12Device* device, DescriptorAllocator& descritptor_allocator, graphics::ConstantBuffer& cb);
    ~ConstantBuffer() override;
    void        UpdateData(std::size_t index, const std::byte* data, std::size_t data_size);
    void        Resize(ID3D12Device* device, DescriptorAllocator& descritptor_allocator, std::size_t new_num_elements);
    const auto& GetCBV(std::size_t index) const { return m_CBV.at(index); }
    const auto& GetDesc() const noexcept { return m_Desc; }

    std::size_t GetBlockSize() const noexcept { return m_BlockSize; }

private:
    std::byte*                    m_CpuPtr = nullptr;
    graphics::ConstantBufferDesc& m_Desc;

    std::size_t                                   m_BlockSize;  // align(dataSize, 256B)
    std::size_t                                   m_BufferSize;
    std::pmr::vector<std::shared_ptr<Descriptor>> m_CBV;
};

class TextureBuffer : public GpuResource {
public:
    TextureBuffer(std::string_view name, ID3D12Device* device, std::shared_ptr<Descriptor> srv, const D3D12_RESOURCE_DESC& desc);
    const auto& GetSRV() const noexcept { return m_SRV; }

private:
    std::shared_ptr<Descriptor> m_SRV;
};

class RenderTarget : public GpuResource {
public:
    RenderTarget(std::string_view name, ID3D12Device* device, std::shared_ptr<Descriptor> rtv, const D3D12_RESOURCE_DESC& desc);
    // Create from exsited resource
    RenderTarget(std::string_view name, ID3D12Device* device, std::shared_ptr<Descriptor> rtv, ID3D12Resource* res);
    const auto& GetRTV() const noexcept { return m_RTV; }

private:
    std::shared_ptr<Descriptor> m_RTV;
};

class DepthBuffer : public GpuResource {
public:
    DepthBuffer(std::string_view name, ID3D12Device* device, std::shared_ptr<Descriptor> dsv, const D3D12_RESOURCE_DESC& desc, float clear_depth, uint8_t clear_stencil);
    const auto& GetDSV() const noexcept { return m_DSV; }

    float   GetClearDepth() const noexcept { return m_ClearDepth; }
    uint8_t GetClearStencil() const noexcept { return m_ClearStencil; }

private:
    std::shared_ptr<Descriptor> m_DSV;
    float                       m_ClearDepth;
    uint8_t                     m_ClearStencil;
};

}  // namespace hitagi::graphics::backend::DX12