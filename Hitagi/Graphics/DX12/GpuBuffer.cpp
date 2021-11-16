#include "GpuBuffer.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Graphics::backend::DX12 {

GpuBuffer::GpuBuffer(ID3D12Device*    device,
                     std::string_view name,
                     size_t           num_elements,
                     size_t           element_size)
    : m_NumElements(num_elements),
      m_ElementSize(element_size),
      m_BufferSize(num_elements * element_size) {
    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize, m_ResourceFlags);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr,
                                                  IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

ConstantBuffer::ConstantBuffer(std::string_view name, ID3D12Device* device, DescriptorAllocator& descritptor_allocator, size_t num_elements, size_t element_size)
    : m_NumElements(num_elements),
      m_ElementSize(element_size),
      m_BlockSize(align(element_size, 256)),
      m_BufferSize(align(element_size, 256) * num_elements) {
    m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;

    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());

    m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_CpuPtr));

    // Create CBV
    assert(descritptor_allocator.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_CBV = descritptor_allocator.Allocate(m_NumElements);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
    cbv_desc.SizeInBytes    = m_BlockSize;
    cbv_desc.BufferLocation = m_Resource->GetGPUVirtualAddress();
    for (auto&& cbv : m_CBV) {
        device->CreateConstantBufferView(&cbv_desc, cbv.handle);
        cbv_desc.BufferLocation += m_BlockSize;
    }
}

void ConstantBuffer::UpdateData(size_t offset, const uint8_t* data, size_t data_size) {
    std::copy_n(data, data_size, m_CpuPtr + offset * m_BlockSize);
}

ConstantBuffer::~ConstantBuffer() {
    m_Resource->Unmap(0, nullptr);
}

TextureBuffer::TextureBuffer(std::string_view name, ID3D12Device* device, Descriptor&& srv, const D3D12_RESOURCE_DESC& desc)
    : m_SRV(std::move(srv)) {
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->CreateShaderResourceView(m_Resource.Get(), nullptr, m_SRV.handle);
}

RenderTarget::RenderTarget(std::string_view name, ID3D12Device* device, Descriptor&& rtv, const D3D12_RESOURCE_DESC& desc)
    : m_RTV(std::move(rtv)) {
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

RenderTarget::RenderTarget(std::string_view name, ID3D12Device* device, Descriptor&& rtv, ID3D12Resource* res)
    : GpuResource(res), m_RTV(std::move(rtv)) {
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

DepthBuffer::DepthBuffer(std::string_view name, ID3D12Device* device, Descriptor&& dsv, const D3D12_RESOURCE_DESC& desc, float clear_depth, uint8_t clear_stencil)
    : m_DSV(std::move(dsv)), m_ClearDepth(clear_depth), m_ClearStencil(clear_stencil) {
    auto              heap_props     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clear_value    = {};
    clear_value.Format               = desc.Format;
    clear_value.DepthStencil.Depth   = m_ClearDepth;
    clear_value.DepthStencil.Stencil = m_ClearStencil;
    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, &clear_value, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format                        = desc.Format;
    dsv_desc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice            = 0;
    dsv_desc.Flags                         = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(m_Resource.Get(), &dsv_desc, m_DSV.handle);
}

}  // namespace Hitagi::Graphics::backend::DX12