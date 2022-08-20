#include "gpu_buffer.hpp"
#include "utils.hpp"
#include "dx12_device.hpp"

#include <hitagi/graphics/gpu_resource.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <numeric>

namespace hitagi::graphics::backend::DX12 {

GpuBuffer::GpuBuffer(DX12Device*           device,
                     std::string_view      name,
                     std::size_t           size,
                     D3D12_RESOURCE_STATES usage)
    : GpuResource(device, nullptr, usage),
      m_BufferSize(size) {
    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize, m_ResourceFlags);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr,
                                                               IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

ConstantBuffer::ConstantBuffer(DX12Device* device, std::string_view name, const graphics::ConstantBuffer& desc)
    : GpuResource(device),
      m_ElementSize(desc.element_size),
      m_BlockSize(utils::align(desc.element_size, 256)),
      m_BufferSize(utils::align(desc.element_size, 256) * desc.num_elements) {
    m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
    Resize(desc.num_elements);
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

void ConstantBuffer::UpdateData(std::size_t index, const std::byte* data, std::size_t data_size) {
    if (sizeof(data_size) > m_BlockSize) {
        spdlog::get("GraphicsManager")->error("ConstantBuffer: the size({}) of input data must be smaller than block size ({})", data_size, m_BlockSize);
        return;
    }
    if (sizeof(data_size) > m_ElementSize) {
        spdlog::get("GraphicsManager")->warn("ConstantBuffer: the size({}) of input data need be smaller than element size ({})", data_size, m_ElementSize);
    }
    if (index >= m_BufferSize / m_BlockSize) {
        spdlog::get("GraphicsManager")->error("ConstantBuffer: the index({}) of data is out of range({})", index, m_BufferSize / m_BlockSize);
        return;
    }
    std::copy_n(data, data_size, m_CpuPtr + index * m_BlockSize);
}

void ConstantBuffer::Resize(std::size_t num_elements) {
    if (m_Resource != nullptr && num_elements == m_BufferSize / m_BlockSize)
        return;

    std::size_t            new_buffer_size = num_elements * m_BlockSize;
    ComPtr<ID3D12Resource> resource;

    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(new_buffer_size);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(m_Device->GetDevice()->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        m_UsageState,
        nullptr,
        IID_PPV_ARGS(&resource)));

    std::byte* new_cpu_ptr = nullptr;
    resource->Map(0, nullptr, reinterpret_cast<void**>(&new_cpu_ptr));

    if (m_Resource) {
        std::array<wchar_t, 128> name{};
        UINT                     size = sizeof(name);
        m_Resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name.data());
        resource->SetName(std::wstring(name.begin(), name.end()).data());

        if (m_CpuPtr) std::copy_n(m_CpuPtr, m_BufferSize, new_cpu_ptr);

        m_Resource->Unmap(0, nullptr);
    }

    m_CpuPtr     = new_cpu_ptr;
    m_Resource   = resource;
    m_BufferSize = new_buffer_size;

    auto& cbv_allocator = m_Device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // Create CBV
    m_CBV = cbv_allocator.Allocate(num_elements, Descriptor::Type::CBV);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
    cbv_desc.SizeInBytes    = m_BlockSize;
    cbv_desc.BufferLocation = m_Resource->GetGPUVirtualAddress();
    for (auto&& cbv : m_CBV) {
        m_Device->GetDevice()->CreateConstantBufferView(&cbv_desc, cbv.handle);
        cbv_desc.BufferLocation += m_BlockSize;
    }
}

ConstantBuffer::~ConstantBuffer() {
    m_Resource->Unmap(0, nullptr);
}

Texture::Texture(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc)
    : GpuResource(device),
      m_SRV(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate(Descriptor::Type::SRV)) {
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->GetDevice()->CreateShaderResourceView(m_Resource.Get(), nullptr, m_SRV.handle);
}

RenderTarget::RenderTarget(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc)
    : GpuResource(device),
      m_RTV(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate(Descriptor::Type::RTV)) {
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->GetDevice()->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

RenderTarget::RenderTarget(DX12Device* device, std::string_view name, ID3D12Resource* res)
    : GpuResource(device, res),
      m_RTV(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate(Descriptor::Type::RTV)) {
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->GetDevice()->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

DepthBuffer::DepthBuffer(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc, float clear_depth, uint8_t clear_stencil)
    : GpuResource(device),
      m_DSV(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV).Allocate(Descriptor::Type::DSV)),
      m_ClearDepth(clear_depth),
      m_ClearStencil(clear_stencil) {
    auto              heap_props     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clear_value    = {};
    clear_value.Format               = desc.Format;
    clear_value.DepthStencil.Depth   = m_ClearDepth;
    clear_value.DepthStencil.Stencil = m_ClearStencil;
    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, &clear_value, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format                        = desc.Format;
    dsv_desc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice            = 0;
    dsv_desc.Flags                         = D3D12_DSV_FLAG_NONE;

    device->GetDevice()->CreateDepthStencilView(m_Resource.Get(), &dsv_desc, m_DSV.handle);
}

}  // namespace hitagi::graphics::backend::DX12