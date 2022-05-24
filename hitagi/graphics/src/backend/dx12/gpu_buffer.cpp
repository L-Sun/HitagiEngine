#include "gpu_buffer.hpp"
#include "hitagi/resource/enums.hpp"
#include "magic_enum.hpp"
#include "utils.hpp"

#include <numeric>

namespace hitagi::graphics::backend::DX12 {

GpuBuffer::GpuBuffer(ID3D12Device*         device,
                     std::string_view      name,
                     std::size_t           size,
                     D3D12_RESOURCE_STATES usage)
    : GpuResource(nullptr, usage),
      m_BufferSize(size) {
    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize, m_ResourceFlags);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr,
                                                  IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

constexpr std::size_t calculate_total_vertex_buffer_size(graphics::VertexBufferDesc desc) {
    std::size_t result = 0;
    for (std::size_t slot = 0; slot < desc.slot_mask.size(); slot++) {
        const bool enabled = desc.slot_mask.test(slot);
        if (enabled) continue;

        const std::size_t attribute_size =
            resource::get_vertex_attribute_size(
                magic_enum::enum_cast<resource::VertexAttribute>(slot).value());

        result += desc.vertex_count * attribute_size;
    }
    return result;
}

VertexBuffer::VertexBuffer(ID3D12Device* device, std::string_view name, graphics::VertexBufferDesc desc)
    : GpuBuffer(device,
                name,
                calculate_total_vertex_buffer_size(desc),
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
      m_Desc(desc) {
}

D3D12_VERTEX_BUFFER_VIEW VertexBuffer::VertexBufferView(std::size_t slot) const {
    assert(SlotEnabled(slot));

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = m_Resource->GetGPUVirtualAddress() + GetSlotOffset(slot);
    vbv.SizeInBytes    = GetSlotSize(slot);
    vbv.StrideInBytes  = GetSlotElementSize(slot);
    return vbv;
}
bool VertexBuffer::SlotEnabled(std::size_t slot) const {
    return m_Desc.slot_mask.test(slot);
}

std::size_t VertexBuffer::GetSlotOffset(std::size_t slot) const {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < slot; i++) {
        offset += GetSlotSize(i);
    }
    return offset;
}

std::size_t VertexBuffer::GetSlotSize(std::size_t slot) const {
    return SlotEnabled(slot) ? m_Desc.vertex_count * GetSlotElementSize(slot) : 0;
}

std::size_t VertexBuffer::GetSlotElementSize(std::size_t slot) const {
    return resource::get_vertex_attribute_size(magic_enum::enum_cast<resource::VertexAttribute>(slot).value());
}

IndexBuffer::IndexBuffer(ID3D12Device* device, std::string_view name, graphics::IndexBufferDesc desc)
    : GpuBuffer(device,
                name,
                desc.index_count * desc.index_size,
                D3D12_RESOURCE_STATE_INDEX_BUFFER),
      m_Desc(desc) {
}

ConstantBuffer::ConstantBuffer(std::string_view name, ID3D12Device* device, DescriptorAllocator& descritptor_allocator, graphics::ConstantBufferDesc desc)
    : m_Desc(desc),
      m_BlockSize(align(desc.element_size, 256)),
      m_BufferSize(align(desc.element_size, 256) * desc.num_elements) {
    m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
    Resize(device, descritptor_allocator, desc.num_elements);
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

void ConstantBuffer::UpdateData(std::size_t index, const std::byte* data, std::size_t data_size) {
    assert(data != nullptr);
    if (data_size > m_Desc.element_size) {
        throw std::invalid_argument(fmt::format("the size of input data must be smaller than block size {}", m_Desc.element_size));
    }
    if (index >= m_Desc.num_elements) {
        throw std::out_of_range("the input data is out of range of constant buffer!");
    }
    std::copy_n(data, data_size, m_CpuPtr + index * m_BlockSize);
}

void ConstantBuffer::Resize(ID3D12Device* device, DescriptorAllocator& descritptor_allocator, std::size_t num_elements) {
    std::size_t            new_buffer_size = num_elements * m_BlockSize;
    ComPtr<ID3D12Resource> resource;

    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(new_buffer_size);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
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

    m_CpuPtr            = new_cpu_ptr;
    m_Resource          = resource;
    m_BufferSize        = new_buffer_size;
    m_Desc.num_elements = num_elements;

    // Create CBV
    assert(descritptor_allocator.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_CBV = descritptor_allocator.Allocate(num_elements);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
    cbv_desc.SizeInBytes    = m_BlockSize;
    cbv_desc.BufferLocation = m_Resource->GetGPUVirtualAddress();
    for (auto&& cbv : m_CBV) {
        device->CreateConstantBufferView(&cbv_desc, cbv.handle);
        cbv_desc.BufferLocation += m_BlockSize;
    }
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

}  // namespace hitagi::graphics::backend::DX12