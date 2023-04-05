#include "dx12_bindless.hpp"
#include "dx12_resource.hpp"
#include "dx12_device.hpp"
#include "dx12_utils.hpp"

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {
// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#levels-of-hardware-support
constexpr std::uint32_t max_cbv_srv_uav_descriptors = 100'0000;
constexpr std::uint32_t max_sampler_descriptors     = 1024;

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
                .index = max_cbv_srv_uav_descriptors - index - 1,
            });
        }
        for (std::uint32_t index = 0; index < max_sampler_descriptors; index++) {
            m_Available_Sampler_BindlessHandlePool.emplace_back(BindlessHandle{
                .index = max_sampler_descriptors - index - 1,
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
        handle = m_Available_CBV_SRV_UAV_BindlessHandlePool.back();
        m_Available_CBV_SRV_UAV_BindlessHandlePool.pop_back();
    }
    handle.type     = BindlessHandleType::Buffer;
    handle.writable = writable ? 1 : 0;

    const auto& dx12_device = static_cast<DX12Device&>(m_Device);
    const auto& dx12_buffer = static_cast<DX12GPUBuffer&>(buffer);

    const auto descriptor_increment_size = dx12_device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx12_device.GetDevice()->CopyDescriptorsSimple(
        1,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            handle.index,
            descriptor_increment_size),
        CD3DX12_CPU_DESCRIPTOR_HANDLE(
            (writable ? dx12_buffer.uavs : dx12_buffer.cbvs).cpu_handle,
            index,
            descriptor_increment_size),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
        handle = m_Available_CBV_SRV_UAV_BindlessHandlePool.back();
        m_Available_CBV_SRV_UAV_BindlessHandlePool.pop_back();
    }

    handle.type     = BindlessHandleType::Texture;
    handle.writable = writable ? 1 : 0;

    const auto& dx12_device  = static_cast<DX12Device&>(m_Device);
    const auto& dx12_texture = static_cast<DX12Texture&>(texture);

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
    return handle;
}

auto DX12BindlessUtils::CreateBindlessHandle(Sampler& sampler) -> BindlessHandle {
    BindlessHandle handle;
    {
        std::lock_guard lock{m_Mutex};
        handle = m_Available_Sampler_BindlessHandlePool.back();
        m_Available_Sampler_BindlessHandlePool.pop_back();
    }
    handle.type = BindlessHandleType::Sampler;

    const auto& dx12_device  = static_cast<DX12Device&>(m_Device);
    const auto& dx12_sampler = static_cast<DX12Sampler&>(sampler);

    const auto descriptor_increment_size = dx12_device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    dx12_device.GetDevice()->CopyDescriptorsSimple(
        1,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_Sampler_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            handle.index,
            descriptor_increment_size),
        dx12_sampler.sampler.cpu_handle,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    return handle;
}

void DX12BindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    std::lock_guard lock{m_Mutex};
    auto&           pool = handle.type == BindlessHandleType::Sampler ? m_Available_Sampler_BindlessHandlePool : m_Available_CBV_SRV_UAV_BindlessHandlePool;

    handle.type     = BindlessHandleType::Invalid;
    handle.writable = 0;
    handle.version++;

    pool.emplace_back(handle);
}

}  // namespace hitagi::gfx