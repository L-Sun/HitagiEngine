module;
#include "interop/win32_macros.hpp"
#include "interop/tracy_macros.hpp"

export module gfx.dx12:bindless;
#ifdef _WIN32
import interop.win32;
#endif
import interop.fmt;
import interop.spdlog;
import interop.tracy;
import interop.dx12;
import interop.magic_enum;

import std;
import core;
import utils;
import math;
import gfx.base;
import :utils;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

class DX12BindlessUtils : public BindlessUtils {
public:
    DX12BindlessUtils(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, std::string_view name);
    auto& GetNativeDevice() const noexcept { return m_Device; }

    auto CreateBindlessHandle(const SamplerDesc& desc) -> BindlessHandle;

    void DiscardBindlessHandle(BindlessHandle handle) final;

    inline auto GetDescriptorHeaps() const noexcept -> std::array<ID3D12DescriptorHeap*, 2> {
        return {
            m_CBV_SRV_UAV_DescriptorHeap.Get(),
            m_Sampler_DescriptorHeap.Get(),
        };
    }
    inline auto GetBindlessRootSignature() const noexcept { return m_RootSignature; }

public:
    auto CreateBindlessHandle(ID3D12Resource& resource, const GPUBufferViewDesc& view_desc, std::uint64_t size) -> BindlessHandle;
    auto CreateBindlessHandle(ID3D12Resource& resource, const TextureViewDesc& view_desc) -> BindlessHandle;

private:
    TracyLockableN(std::mutex, m_Mutex, "DX12 Bindless Mutex");

    ID3D12Device&                   m_Device;
    std::shared_ptr<spdlog::logger> m_Logger;
    ComPtr<ID3D12RootSignature>     m_RootSignature;

    ComPtr<ID3D12DescriptorHeap> m_CBV_SRV_UAV_DescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> m_Sampler_DescriptorHeap;

    std::pmr::deque<BindlessHandle> m_Available_CBV_SRV_UAV_BindlessHandlePool;
    std::pmr::deque<BindlessHandle> m_Available_Sampler_BindlessHandlePool;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#levels-of-hardware-support
static constexpr std::uint32_t max_cbv_srv_uav_descriptors = 3'0000;
static constexpr std::uint32_t max_sampler_descriptors     = 128;

DX12BindlessUtils::DX12BindlessUtils(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, std::string_view name) : BindlessUtils(name), m_Device(device), m_Logger(std::move(logger)) {
    logger = m_Logger;
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
        if (interop::failed(D3DX12SerializeVersionedRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error))) {
            const auto error_message = fmt::format(
                "Failed to serialize RootSignature({})",
                fmt::styled(name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }

        if (interop::failed(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)))) {
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
    device.CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(&m_CBV_SRV_UAV_DescriptorHeap));
    m_CBV_SRV_UAV_DescriptorHeap->SetName(L"BindlessDescriptorHeap");

    const D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc{
        .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        .NumDescriptors = max_sampler_descriptors,
        .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask       = 0,
    };
    device.CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(&m_Sampler_DescriptorHeap));
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

auto DX12BindlessUtils::CreateBindlessHandle(ID3D12Resource& resource, const GPUBufferViewDesc& view_desc, std::uint64_t size) -> BindlessHandle {
    const auto  view_type  = view_desc.type;
    const auto writable   = view_type == GPUBufferViewType::StorageWrite;

    BindlessHandle handle;
    {
        std::scoped_lock lock{m_Mutex};
        if (m_Available_CBV_SRV_UAV_BindlessHandlePool.empty()) throw std::runtime_error("Bindless descriptor heap exhausted");
        handle = m_Available_CBV_SRV_UAV_BindlessHandlePool.front();
        m_Available_CBV_SRV_UAV_BindlessHandlePool.pop_front();
    }
    handle.type     = BindlessHandleType::Buffer;
    handle.writable = writable ? 1 : 0;

    const auto descriptor_increment_size  = m_Device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto descriptor_cpu_handle      = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        handle.index,
        descriptor_increment_size);

    if (writable) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
            .Format        = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer        = {
                .FirstElement        = view_desc.offset / 4,
                .NumElements         = static_cast<UINT>(size / 4),
                .StructureByteStride = 0,
                .Flags               = D3D12_BUFFER_UAV_FLAG_RAW,
            },
        };
        m_Device.CreateUnorderedAccessView(
            &resource,
            nullptr,
            &uav_desc,
            descriptor_cpu_handle);
    } else {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format                  = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = interop::default_shader_4_component_mapping,
            .Buffer                  = {
                .FirstElement        = view_desc.offset / 4,
                .NumElements         = static_cast<UINT>(size / 4),
                .StructureByteStride = 0,
                .Flags               = D3D12_BUFFER_SRV_FLAG_RAW,
            },
        };
        m_Device.CreateShaderResourceView(
            &resource,
            &srv_desc,
            descriptor_cpu_handle);
    }

    return handle;
}

auto DX12BindlessUtils::CreateBindlessHandle(ID3D12Resource& resource, const TextureViewDesc& view_desc) -> BindlessHandle {
    auto&       texture   = *view_desc.texture;
    const auto  writable  = view_desc.type == TextureViewType::ShaderWrite;

    if (writable && !utils::has_flag(texture.GetDesc().usages, TextureUsageFlags::UAV)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: texture({}) is not writable",
            fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
        m_Logger->error(error_message);
        throw std::invalid_argument(error_message);
    }
    if (!writable && !utils::has_flag(texture.GetDesc().usages, TextureUsageFlags::SRV)) {
        const auto error_message = fmt::format(
            "Failed to create BindlessHandle: texture({}) is not used for shader visible",
            fmt::styled(texture.GetName(), fmt::fg(fmt::color::red)));
        m_Logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    BindlessHandle handle;
    {
        std::scoped_lock lock{m_Mutex};
        if (m_Available_CBV_SRV_UAV_BindlessHandlePool.empty()) throw std::runtime_error("Bindless descriptor heap exhausted");
        handle = m_Available_CBV_SRV_UAV_BindlessHandlePool.front();
        m_Available_CBV_SRV_UAV_BindlessHandlePool.pop_front();
    }

    handle.type     = BindlessHandleType::Texture;
    handle.writable = writable ? 1 : 0;

    const auto descriptor_increment_size = m_Device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (writable) {
        const auto uav_desc = to_d3d_uav_desc(view_desc);
        m_Device.CreateUnorderedAccessView(
            &resource,
            nullptr,
            &uav_desc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                handle.index,
                descriptor_increment_size));
    } else {
        const auto srv_desc = to_d3d_srv_desc(view_desc);
        m_Device.CreateShaderResourceView(
            &resource,
            &srv_desc,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_CBV_SRV_UAV_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                handle.index,
                descriptor_increment_size));
    }

    return handle;
}

auto DX12BindlessUtils::CreateBindlessHandle(const SamplerDesc& desc) -> BindlessHandle {
    BindlessHandle handle;
    {
        std::scoped_lock lock{m_Mutex};
        if (m_Available_Sampler_BindlessHandlePool.empty()) throw std::runtime_error("Bindless descriptor heap exhausted");
        handle = m_Available_Sampler_BindlessHandlePool.front();
        m_Available_Sampler_BindlessHandlePool.pop_front();
    }
    handle.type = BindlessHandleType::Sampler;

    const auto descriptor_increment_size = m_Device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    const auto sampler_desc = to_d3d_sampler_desc(desc);

    m_Device.CreateSampler(
        &sampler_desc,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_Sampler_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            handle.index,
            descriptor_increment_size));

    return handle;
}

void DX12BindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    if (handle.type == BindlessHandleType::Invalid) return;

    std::scoped_lock lock{m_Mutex};
    auto&            pool = handle.type == BindlessHandleType::Sampler ? m_Available_Sampler_BindlessHandlePool : m_Available_CBV_SRV_UAV_BindlessHandlePool;

    handle.type     = BindlessHandleType::Invalid;
    handle.writable = 0;
    handle.version++;

    pool.emplace_back(handle);
}

}  // namespace hitagi::gfx
