#include "dx12_bindless.hpp"
#include "dx12_resource.hpp"
#include "dx12_device.hpp"

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {
// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#levels-of-hardware-support
constexpr std::uint32_t max_cbv_srv_uav_descriptors = 1000000;
constexpr std::uint32_t max_sampler_descriptors     = 1024;

DX12BindlessUtils::DX12BindlessUtils(DX12Device& device, std::string_view name)
    : BindlessUtils(device, name),
      m_CBV_SRV_UAV_DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, max_cbv_srv_uav_descriptors, "BindlessDescriptorHeap"),
      m_Sampler_DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true, max_sampler_descriptors, "BindlessDescriptorHeap") {
    const auto logger = device.GetLogger();
    logger->trace("Creating BindlessUtils: {}", fmt::styled(name, fmt::fg(fmt::color::green)));

    logger->trace("Create bindless root signature...");
    {
        const CD3DX12_DESCRIPTOR_RANGE cbv_range{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0};

        CD3DX12_ROOT_PARAMETER push_constant_root_parameter;
        push_constant_root_parameter.InitAsConstants(sizeof(BindlessInfoOffset) / sizeof(std::uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_ALL);

        const CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc(
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
        if (FAILED(D3D12SerializeRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
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

    logger->trace("Allocate Bindless Handles");
    {
        m_CBV_SRV_UAV_Descriptors = m_CBV_SRV_UAV_DescriptorHeap.Allocate(m_CBV_SRV_UAV_DescriptorHeap.AvailableSize());
        m_Sampler_Descriptors     = m_Sampler_DescriptorHeap.Allocate(m_Sampler_DescriptorHeap.AvailableSize());
        for (std::size_t index = 0; index < m_CBV_SRV_UAV_Descriptors.num; index++) {
            m_Available_CBV_SRV_UAV_BindlessHandles.emplace_back(BindlessHandle{
                .index   = static_cast<std::uint32_t>(index),
                .version = 0,
            });  // ! we do not specify type here, but dynamically specify type on CreateBindlessHandle
        }
        for (std::size_t index = 0; index < m_Sampler_Descriptors.num; index++) {
            m_Available_Sampler_BindlessHandles.emplace_back(BindlessHandle{
                .index   = static_cast<std::uint32_t>(index),
                .version = 0,
                .type    = 3,
            });
        }
    }
}

auto DX12BindlessUtils::CreateBindlessHandle(GPUBuffer& buffer, bool writable) -> BindlessHandle {
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
        handle = m_Available_CBV_SRV_UAV_BindlessHandles.back();
        m_Available_CBV_SRV_UAV_BindlessHandles.pop_back();
    }
    handle.type = 0;
    handle.tag  = writable ? 1 : 0;

    const auto& dx12_device = static_cast<DX12Device&>(m_Device);
    const auto& dx12_buffer = static_cast<DX12GPUBuffer&>(buffer);
    dx12_device.GetDevice()->CopyDescriptorsSimple(
        1,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CBV_SRV_UAV_Descriptors.cpu_handle, handle.index, m_CBV_SRV_UAV_Descriptors.increment_size),
        (writable ? dx12_buffer.uav : dx12_buffer.cbv).cpu_handle,
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
        handle = m_Available_CBV_SRV_UAV_BindlessHandles.back();
        m_Available_CBV_SRV_UAV_BindlessHandles.pop_back();
    }

    handle.type = 1;
    handle.tag  = writable ? 1 : 0;

    const auto& dx12_device  = static_cast<DX12Device&>(m_Device);
    const auto& dx12_texture = static_cast<DX12Texture&>(texture);
    dx12_device.GetDevice()->CopyDescriptorsSimple(
        1,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CBV_SRV_UAV_Descriptors.cpu_handle, handle.index, m_CBV_SRV_UAV_Descriptors.increment_size),
        (writable ? dx12_texture.uav : dx12_texture.srv).cpu_handle,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return handle;
}

void DX12BindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    std::lock_guard lock{m_Mutex};

    if (handle.type == 0 || handle.type == 1) {
        handle.version++;
        m_Available_CBV_SRV_UAV_BindlessHandles.emplace_back(handle);
    } else if (handle.type == 3) {
        handle.version++;
        m_Available_Sampler_BindlessHandles.emplace_back(handle);
    }
}

}  // namespace hitagi::gfx