#pragma once
#include "gpu_resource.hpp"
#include "descriptor_allocator.hpp"

namespace hitagi::gfx::backend::DX12 {
class DX12Device;

class GpuBuffer : public GpuResource {
public:
    enum struct Usage : std::uint8_t {
        Default,   // CPU no access, GPU read/write
        Upload,    // CPU write, GPU read
        Readback,  // CPU read, GPU write
    };

    GpuBuffer(
        DX12Device*          device,
        std::string_view     name,
        std::size_t          size,
        Usage                usage = Usage::Default,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    std::size_t GetBufferSize() const { return m_BufferSize; }

protected:
    std::size_t          m_BufferSize;
    D3D12_RESOURCE_FLAGS m_ResourceFlags;
};

class ConstantBuffer : public GpuBuffer {
public:
    ConstantBuffer(DX12Device* device, std::string_view name, gfx::ConstantBuffer& desc);

    ~ConstantBuffer() override;

    void        UpdateData(std::size_t index, const std::byte* data, std::size_t data_size);
    void        Resize(std::size_t new_num_elements);
    const auto& GetCBV(std::size_t index) const { return m_CBV.at(index); }
    inline auto GetBlockSize(std::size_t index) const noexcept { return m_BlockSize; }

private:
    std::byte* m_CpuPtr = nullptr;

    std::size_t                  m_NumElements;
    std::size_t                  m_ElementSize;
    std::size_t                  m_BlockSize;
    std::pmr::vector<Descriptor> m_CBV;
};

class Texture : public GpuResource {
public:
    Texture(DX12Device* device, const resource::Texture& texture, ID3D12Resource* resource = nullptr);
    const auto& GetRTV() const noexcept { return m_RTV; }
    const auto& GetDSV() const noexcept { return m_DSV; }
    const auto& GetSRV() const noexcept { return m_SRV; }
    const auto& GetUAV() const noexcept { return m_UAV; }

private:
    Descriptor m_SRV;
    Descriptor m_UAV;
    Descriptor m_RTV;
    Descriptor m_DSV;
};

}  // namespace hitagi::gfx::backend::DX12