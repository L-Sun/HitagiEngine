#include "GpuBuffer.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Graphics::backend::DX12 {

GpuBuffer::GpuBuffer(ID3D12Device*    device,
                     std::string_view name,
                     size_t           numElements,
                     size_t           elementSize)
    : GpuResource(name),
      m_NumElements(numElements),
      m_ElementSize(elementSize),
      m_BufferSize(numElements * elementSize) {
    auto desc      = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize, m_ResourceFlags);
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr,
                                                  IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

ConstantBuffer::ConstantBuffer(std::string_view name, ID3D12Device* device, DescriptorAllocator& descritptorAllocator, size_t numElements, size_t elementSize)
    : GpuResource(name),
      m_NumElements(numElements),
      m_ElementSize(elementSize),
      m_BlockSize(align(elementSize, 256)),
      m_BufferSize(align(elementSize, 256) * numElements) {
    m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;

    auto desc      = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());

    m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_CpuPtr));

    // Create CBV
    assert(descritptorAllocator.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_CBV = descritptorAllocator.Allocate(m_NumElements);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.SizeInBytes    = m_BlockSize;
    cbvDesc.BufferLocation = m_Resource->GetGPUVirtualAddress();
    for (auto&& cbv : m_CBV) {
        device->CreateConstantBufferView(&cbvDesc, cbv.handle);
        cbvDesc.BufferLocation += m_BlockSize;
    }
}

void ConstantBuffer::UpdataData(size_t offset, const uint8_t* data, size_t dataSize) {
    std::copy_n(data, dataSize, m_CpuPtr + offset);
}

ConstantBuffer::~ConstantBuffer() {
    m_Resource->Unmap(0, nullptr);
}

TextureBuffer::TextureBuffer(std::string_view name, ID3D12Device* device, Descriptor&& srv, const D3D12_RESOURCE_DESC& desc)
    : GpuResource(name), m_SRV(std::move(srv)) {
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->CreateShaderResourceView(m_Resource.Get(), nullptr, m_SRV.handle);
}

RenderTarget::RenderTarget(std::string_view name, ID3D12Device* device, Descriptor&& rtv, const D3D12_RESOURCE_DESC& desc)
    : GpuResource(name), m_RTV(std::move(rtv)) {
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

RenderTarget::RenderTarget(std::string_view name, ID3D12Device* device, Descriptor&& rtv, ID3D12Resource* res)
    : GpuResource(name, res), m_RTV(std::move(rtv)) {
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

DepthBuffer::DepthBuffer(std::string_view name, ID3D12Device* device, Descriptor&& dsv, const D3D12_RESOURCE_DESC& desc, float clearDepth, uint8_t clearStencil)
    : GpuResource(name), m_DSV(std::move(dsv)), m_ClearDepth(clearDepth), m_ClearStencil(clearStencil) {
    auto              heapProps     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clearValue    = {};
    clearValue.Format               = desc.Format;
    clearValue.DepthStencil.Depth   = m_ClearDepth;
    clearValue.DepthStencil.Stencil = m_ClearStencil;
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, &clearValue, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format                        = desc.Format;
    dsvDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice            = 0;
    dsvDesc.Flags                         = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(m_Resource.Get(), &dsvDesc, m_DSV.handle);
}

}  // namespace Hitagi::Graphics::backend::DX12