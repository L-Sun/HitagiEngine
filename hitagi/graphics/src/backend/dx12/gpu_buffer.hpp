#pragma once
#include "gpu_resource.hpp"
#include "descriptor_allocator.hpp"

namespace hitagi::graphics::backend::DX12 {
class DX12Device;

class GpuBuffer : public GpuResource {
public:
    GpuBuffer(DX12Device* device, std::string_view name, std::size_t size, D3D12_RESOURCE_STATES usage = D3D12_RESOURCE_STATE_COMMON);
    std::size_t GetBufferSize() const { return m_BufferSize; }

protected:
    std::size_t          m_BufferSize    = 0;
    D3D12_RESOURCE_FLAGS m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
};

class ConstantBuffer : public GpuResource {
public:
    ConstantBuffer(DX12Device* device, std::string_view name, const graphics::ConstantBuffer& desc);

    ~ConstantBuffer() override;

    void        UpdateData(std::size_t index, const std::byte* data, std::size_t data_size);
    void        Resize(std::size_t new_num_elements);
    const auto& GetCBV(std::size_t index) const { return m_CBV.at(index); }

    std::size_t GetBlockSize() const noexcept { return m_BlockSize; }

private:
    std::byte* m_CpuPtr = nullptr;

    std::size_t                  m_ElementSize;
    std::size_t                  m_BlockSize;  // align(dataSize, 256B)
    std::size_t                  m_BufferSize;
    std::pmr::vector<Descriptor> m_CBV;
};

class Texture : public GpuResource {
public:
    Texture(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc);
    const auto& GetSRV() const noexcept { return m_SRV; }

private:
    Descriptor m_SRV;
};

class RenderTarget : public GpuResource {
public:
    RenderTarget(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc);
    // Create from exsited resource
    RenderTarget(DX12Device* device, std::string_view name, ID3D12Resource* res);
    const auto& GetRTV() const noexcept { return m_RTV; }

private:
    Descriptor m_RTV;
};

class DepthBuffer : public GpuResource {
public:
    DepthBuffer(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc, float clear_depth, uint8_t clear_stencil);
    const auto& GetDSV() const noexcept { return m_DSV; }

    float   GetClearDepth() const noexcept { return m_ClearDepth; }
    uint8_t GetClearStencil() const noexcept { return m_ClearStencil; }

private:
    Descriptor m_DSV;
    float      m_ClearDepth;
    uint8_t    m_ClearStencil;
};

}  // namespace hitagi::graphics::backend::DX12