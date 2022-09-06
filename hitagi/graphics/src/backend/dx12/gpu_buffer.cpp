#include "gpu_buffer.hpp"
#include "dx12_device.hpp"
#include "utils.hpp"

#include <hitagi/graphics/gpu_resource.hpp>

#include <d3d12.h>
#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <numeric>

namespace hitagi::graphics::backend::DX12 {

GpuBuffer::GpuBuffer(DX12Device* device, std::string_view name, std::size_t size, Usage usage, D3D12_RESOURCE_FLAGS flags)
    : GpuResource(device, nullptr), m_BufferSize(size), m_ResourceFlags(flags) {
    CD3DX12_HEAP_PROPERTIES heap_props;
    switch (usage) {
        case Usage::Default: {
            heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        } break;
        case Usage::Readback: {
            heap_props      = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
            m_ResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
            m_ResourceFlags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        } break;
        case Usage::Upload: {
            heap_props      = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            m_ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        } break;
    }
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        m_ResourceState,
        nullptr,
        IID_PPV_ARGS(&m_Resource)));

    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
}

ConstantBuffer::ConstantBuffer(DX12Device* device, std::string_view name, graphics::ConstantBuffer& desc)
    : GpuBuffer(device, name, desc.num_elements * utils::align(desc.element_size, 256), Usage::Upload),
      m_NumElements(desc.num_elements),
      m_ElementSize(desc.element_size),
      m_BlockSize(utils::align(desc.element_size, 256)) {
    ThrowIfFailed(m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_CpuPtr)));

    if (!desc.cpu_buffer.Empty()) {
        std::copy_n(desc.cpu_buffer.GetData(), desc.cpu_buffer.GetDataSize(), m_CpuPtr);
    }

    // Create CBV
    auto& cbv_allocator = m_Device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_CBV               = cbv_allocator.Allocate(m_NumElements, Descriptor::Type::CBV);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
    cbv_desc.SizeInBytes    = m_BlockSize;
    cbv_desc.BufferLocation = m_Resource->GetGPUVirtualAddress();
    for (auto&& cbv : m_CBV) {
        m_Device->GetDevice()->CreateConstantBufferView(&cbv_desc, cbv.handle);
        cbv_desc.BufferLocation += m_BlockSize;
    }

    desc.update_fn = [this](auto&&... args) { UpdateData(std::forward<decltype(args)>(args)...); };
    desc.resize_fn = [this](auto&&... args) { Resize(std::forward<decltype(args)>(args)...); };
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
    if (num_elements == m_NumElements) return;

    std::size_t            new_buffer_size = num_elements * m_BlockSize;
    ComPtr<ID3D12Resource> resource;

    auto desc       = CD3DX12_RESOURCE_DESC::Buffer(new_buffer_size);
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(m_Device->GetDevice()->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        m_ResourceState,
        nullptr,
        IID_PPV_ARGS(&resource)));

    std::byte* new_cpu_ptr = nullptr;
    resource->Map(0, nullptr, reinterpret_cast<void**>(&new_cpu_ptr));

    // Copy orign data
    std::array<wchar_t, 128> name{};
    UINT                     size = sizeof(name);
    m_Resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name.data());
    resource->SetName(std::wstring(name.begin(), name.end()).data());
    if (m_CpuPtr) std::copy_n(m_CpuPtr, m_BufferSize, new_cpu_ptr);
    m_Resource->Unmap(0, nullptr);

    m_CpuPtr      = new_cpu_ptr;
    m_Resource    = resource;
    m_NumElements = num_elements;
    m_BufferSize  = new_buffer_size;

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

Texture::Texture(DX12Device* device, const resource::Texture& texture) : GpuResource(device) {
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
        to_dxgi_format(texture.format),
        texture.width,
        texture.height,
        texture.array_size,
        texture.mip_level,
        texture.sample_count,
        texture.sample_quality);

    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::DepthBuffer)) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (!utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::ShaderResource)) {
            desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
    }
    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::RenderTarget)) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::UnorderedAccess)) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    CD3DX12_CLEAR_VALUE optimized_clear_value;
    optimized_clear_value.Color[0]             = texture.clear_value.color[0];
    optimized_clear_value.Color[1]             = texture.clear_value.color[1];
    optimized_clear_value.Color[2]             = texture.clear_value.color[2];
    optimized_clear_value.Color[3]             = texture.clear_value.color[3];
    optimized_clear_value.DepthStencil.Depth   = texture.clear_value.depth_stencil.depth;
    optimized_clear_value.DepthStencil.Stencil = texture.clear_value.depth_stencil.stencil;
    optimized_clear_value.Format               = to_dxgi_format(texture.format);

    if (optimized_clear_value.Format == DXGI_FORMAT_R16_TYPELESS) {
        optimized_clear_value.Format = DXGI_FORMAT_D16_UNORM;
    } else if (optimized_clear_value.Format == DXGI_FORMAT_R32_TYPELESS) {
        optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
    } else if (optimized_clear_value.Format == DXGI_FORMAT_R32G8X24_TYPELESS) {
        optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    }
    bool use_clear_value = utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::RenderTarget) || utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::DepthBuffer);

    m_ResourceState = D3D12_RESOURCE_STATE_COMMON;

    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        m_ResourceState,
        use_clear_value ? &optimized_clear_value : nullptr,
        IID_PPV_ARGS(&m_Resource)));

    if (!texture.name.empty())
        m_Resource->SetName(std::wstring(texture.name.begin(), texture.name.end()).data());

    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::ShaderResource)) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format                          = desc.Format;
        srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        // TODO support diffrent dimension texture
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        switch (texture.format) {
            case resource::Format::R16_TYPELESS:
                srv_desc.Format = DXGI_FORMAT_R16_UNORM;
                break;
            case resource::Format::R32_TYPELESS:
                srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
                break;
            case resource::Format::R24G8_TYPELESS:
                srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                break;
            case resource::Format::R32G8X24_TYPELESS:
                srv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                break;
            default:
                srv_desc.Format = to_dxgi_format(texture.format);
                break;
        }

        if (texture.sample_count > 1) {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        } else {
            srv_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels       = -1;
            srv_desc.Texture2D.PlaneSlice      = 0;
        }

        m_SRV = m_Device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate(Descriptor::Type::SRV);
        device->GetDevice()->CreateShaderResourceView(m_Resource.Get(), &srv_desc, m_SRV.handle);
    }
    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::UnorderedAccess)) {
        // m_UAV = m_Device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Allocate(Descriptor::Type::UAV);
        // device->GetDevice()->CreateUnorderedAccessView(m_Resource.Get(), nullptr, m_UAV.handle);
    }
    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::RenderTarget)) {
        m_RTV = m_Device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate(Descriptor::Type::RTV);
        device->GetDevice()->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
    }
    if (utils::has_flag(texture.bind_flags, resource::Texture::BindFlag::DepthBuffer)) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
        dsv_desc.Format             = desc.Format;
        dsv_desc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Texture2D.MipSlice = 0;
        dsv_desc.Flags              = D3D12_DSV_FLAG_NONE;

        m_DSV = m_Device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).Allocate(Descriptor::Type::DSV);
        device->GetDevice()->CreateDepthStencilView(m_Resource.Get(), &dsv_desc, m_DSV.handle);
    }
}

RenderTarget::RenderTarget(DX12Device* device, std::string_view name, const D3D12_RESOURCE_DESC& desc)
    : GpuResource(device),
      m_RTV(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate(Descriptor::Type::RTV)) {
    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, m_ResourceState, nullptr, IID_PPV_ARGS(&m_Resource)));
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->GetDevice()->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

RenderTarget::RenderTarget(DX12Device* device, std::string_view name, ID3D12Resource* res)
    : GpuResource(device, res),
      m_RTV(device->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV).Allocate(Descriptor::Type::RTV)) {
    m_Resource->SetName(std::wstring(name.begin(), name.end()).data());
    device->GetDevice()->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTV.handle);
}

}  // namespace hitagi::graphics::backend::DX12