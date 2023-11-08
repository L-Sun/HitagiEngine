#include "dx12_bindless.hpp"
#include "dx12_resource.hpp"
#include "dx12_device.hpp"
#include "dx12_utils.hpp"
#include <hitagi/utils/utils.hpp>

#include <spdlog/logger.h>
#include <fmt/color.h>
#include <tracy/Tracy.hpp>

namespace hitagi::gfx {
// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#levels-of-hardware-support
constexpr std::uint32_t max_cbv_srv_uav_descriptors = 3'0000;
constexpr std::uint32_t max_sampler_descriptors     = 128;

DX12BindlessUtils::DX12BindlessUtils(DX12Device& device, std::string_view name) : BindlessUtils(device, name) {
    const auto logger = device.GetLogger();
    logger->trace("Creating BindlessUtils: {}", fmt::styled(name, fmt::fg(fmt::color::green)));

    logger->trace("Create bindless root signature...");
    {
        CD3DX12_ROOT_PARAMETER1 push_constant_root_parameter;
        push_constant_root_parameter.InitAsConstants(sizeof(BindlessMetaInfo) / sizeof(std::uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc;
        root_sig_desc.Init_1_1(
            1,
            &push_constant_root_parameter,
            0,
            nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);

        // compile root signature
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3DX12SerializeVersionedRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error))) {
            const auto error_message = fmt::format(
                "Failed to serialize RootSignature({})",
                fmt::styled(name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }

        if (FAILED(device.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)))) {
            const auto error_message = fmt::format(
                "Failed to create RootSignature({})",
                fmt::styled(name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        m_RootSignature->SetName(std::wstring(name.begin(), name.end()).c_str());
    }

    const D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc{
        .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = max_cbv_srv_uav_descriptors,
        .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask       = 0,
    };
    device.GetDevice()->CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(&m_CBV_SRV_UAV_DescriptorHeap));
    m_CBV_SRV_UAV_DescriptorHeap->SetName(L"BindlessDescriptorHeap");

    const D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc{
        .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        .NumDescriptors = max_sampler_descriptors,
        .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask       = 0,
    };
    device.GetDevice()->CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(&m_Sampler_DescriptorHeap));
    m_Sampler_DescriptorHeap->SetName(L"BindlessDescriptorHeap");

    logger->trace("Allocate Bindless Handles");
    {
        for (std::uint32_t index = 0; index < max_cbv_srv_uav_descriptors; index++) {
            m_Available_CBV_SRV_UAV_BindlessHandlePool.emplace_back(BindlessHandle{
                .index = index,
            });
        }
        for (std::uint32_t index = 0; index < max_sampler_descriptors; index++) {
            m_Available_Sampler_BindlessHandlePool.emplace_back(BindlessHandle{
                .index = index,
            });
        }
    }
}

auto DX12BindlessUtils::CreateBindlessHandle(GPUBuffer& buffer, std::size_t index, bool writable) -> BindlessHandle {
    if (writable && !utils::has_flag(buffer.GetDesc().usages, GPUBufferUsageFlags::Storage)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: buffer({}) is not writable",
            fmt::styled(buffer.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }
    if (!writable && !utils::has_flag(buffer.GetDesc().usages, GPUBufferUsageFlags::Constant)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: buffer({}) is not used for shader visible",
            fmt::styled(buffer.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    BindlessHandle handle;
    {
        std::lock_guard lock{m_Mutex};
        handle = m_Available_CBV_SRV_UAV_BindlessHandlePool.front();
        m_Available_CBV_SRV_UAV_BindlessHandlePool.pop_front();
    }
    handle.type     = BindlessHandleType::Buffer;
    handle.writable = writable ? 1 : 0;

    const auto& dx12_device = static_cast<DX12Device&>(m_Device);
    const auto& dx12_buffer = dynamic_cast<DX12GPUBuffer&>(buffer);

    const auto descriptor_increment_size = dx12_device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (writable) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer        = {
                       .FirstElement        = 0,
                       .NumElements         = static_cast<UINT>(buffer.GetDesc().element_count),
                       .StructureByteStride = static_cast<UINT>(buffer.GetDesc().element_size),
                       .Flags               = D3D12_BUFFER_UAV_FLAG_NONE,
            },
        };
        dx12_device.GetDevice()->CreateUnorderedAccessView(
            dx12_buffer.resource.Get(),
            nullptr,
            &uav_desc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                handle.index,
                descriptor_increment_size));
    } else {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {
            .BufferLocation = dx12_buffer.resource->GetGPUVirtualAddress() + index * buffer.AlignedElementSize(),
            .SizeInBytes    = static_cast<UINT>(buffer.AlignedElementSize()),
        };
        dx12_device.GetDevice()->CreateConstantBufferView(
            &cbv_desc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                handle.index,
                descriptor_increment_size));
    }

    TracyAllocN((void*)handle.index, 1, "CBV_SRV_UAV_Bindless");
    return handle;
}

auto DX12BindlessUtils::CreateBindlessHandle(Texture& texture, bool writable) -> BindlessHandle {
    if (writable && !utils::has_flag(texture.GetDesc().usages, TextureUsageFlags::UAV)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: texture({}) is not writable",
            fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }
    if (!writable && !utils::has_flag(texture.GetDesc().usages, TextureUsageFlags::SRV)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: texture({}) is not used for shader visible",
            fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
        m_Device.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    BindlessHandle handle;
    {
        std::lock_guard lock{m_Mutex};
        handle = m_Available_CBV_SRV_UAV_BindlessHandlePool.front();
        m_Available_CBV_SRV_UAV_BindlessHandlePool.pop_front();
    }

    handle.type     = BindlessHandleType::Texture;
    handle.writable = writable ? 1 : 0;

    const auto& dx12_device  = static_cast<DX12Device&>(m_Device);
    const auto& dx12_texture = dynamic_cast<DX12Texture&>(texture);

    const auto descriptor_increment_size = dx12_device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (writable) {
        const auto uav_desc = to_d3d_uav_desc(dx12_texture.GetDesc());
        dx12_device.GetDevice()->CreateUnorderedAccessView(
            dx12_texture.resource.Get(),
            nullptr,
            &uav_desc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                handle.index,
                descriptor_increment_size));
    } else {
        const auto srv_desc = to_d3d_srv_desc(dx12_texture.GetDesc());
        dx12_device.GetDevice()->CreateShaderResourceView(
            dx12_texture.resource.Get(),
            &srv_desc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                handle.index,
                descriptor_increment_size));
    }

    TracyAllocN((void*)handle.index, 1, "CBV_SRV_UAV_Bindless");
    return handle;
}

auto DX12BindlessUtils::CreateBindlessHandle(Sampler& sampler) -> BindlessHandle {
    BindlessHandle handle;
    {
        std::lock_guard lock{m_Mutex};
        handle = m_Available_Sampler_BindlessHandlePool.front();
        m_Available_Sampler_BindlessHandlePool.pop_front();
    }
    handle.type = BindlessHandleType::Sampler;

    const auto& dx12_device = static_cast<DX12Device&>(m_Device);

    const auto descriptor_increment_size = dx12_device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    const auto sampler_desc = to_d3d_sampler_desc(sampler.GetDesc());

    dx12_device.GetDevice()->CreateSampler(
        &sampler_desc,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_Sampler_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            handle.index,
            descriptor_increment_size));

    TracyAllocN((void*)handle.index, 1, "Sampler_Bindless");
    return handle;
}

void DX12BindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    if (handle.type == BindlessHandleType::Invalid) return;

    std::lock_guard lock{m_Mutex};
    auto&           pool = handle.type == BindlessHandleType::Sampler ? m_Available_Sampler_BindlessHandlePool : m_Available_CBV_SRV_UAV_BindlessHandlePool;

    if (handle.type == BindlessHandleType::Sampler) {
        TracyFreeN((void*)handle.index, "Sampler_Bindless");
    } else {
        TracyFreeN((void*)handle.index, "CBV_SRV_UAV_Bindless");
    }

    handle.type     = BindlessHandleType::Invalid;
    handle.writable = 0;
    handle.version++;

    pool.emplace_back(handle);
}

}  // namespace hitagi::gfx