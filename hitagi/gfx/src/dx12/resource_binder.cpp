#include "resource_binder.hpp"
#include "dx12_command_context.hpp"
#include "dx12_device.hpp"
#include "utils.hpp"

#include <d3d12.h>
#include <fmt/color.h>

namespace hitagi::gfx {
ResourceBinder::ResourceBinder(DX12CommandContext& context)
    : m_Context(context) {}

void ResourceBinder::Reset() {
    m_UsedHeaps.clear();
    m_TableHeapOffset = utils::create_array<std::size_t, sm_MaxRootParameters>(std::numeric_limits<std::size_t>::max());
    m_TableMask.reset();
    m_SlotInfos             = utils::create_enum_array<std::pmr::unordered_map<std::uint32_t, SlotInfo>, SlotType>({});
    m_CurrRootSignatureDesc = nullptr;
}

void ResourceBinder::SetRootSignature(const D3D12_ROOT_SIGNATURE_DESC1* root_sig_desc) {
    if (root_sig_desc == nullptr) {
        throw std::invalid_argument("Can not set empty root signature!");
    }
    if (root_sig_desc == m_CurrRootSignatureDesc) return;

    // Reset state but do not clear the used heaps
    m_CurrRootSignatureDesc = root_sig_desc;
    m_TableHeapOffset       = utils::create_array<std::size_t, sm_MaxRootParameters>(std::numeric_limits<std::size_t>::max());
    m_TableMask.reset();
    m_SlotInfos = utils::create_enum_array<std::pmr::unordered_map<std::uint32_t, SlotInfo>, SlotType>({});

    if (m_CurrRootSignatureDesc->NumParameters > sm_MaxRootParameters) {
        auto err_msg = fmt::format("Too much root paramters(Num: {}) are set. It supports up to {} root parameters.", m_CurrRootSignatureDesc->NumParameters, sm_MaxRootParameters);
        m_Context.m_Device.GetLogger()->error(err_msg);
        throw std::runtime_error(err_msg.c_str());
    }

    std::array<std::size_t, 2> curr_heap_offset = {0, 0};
    for (std::uint32_t param_index = 0; param_index < m_CurrRootSignatureDesc->NumParameters; param_index++) {
        const auto& root_param = m_CurrRootSignatureDesc->pParameters[param_index];
        switch (root_param.ParameterType) {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
                const auto& table = root_param.DescriptorTable;
                assert(table.pDescriptorRanges != nullptr);

                for (std::uint32_t range_index = 0; range_index < table.NumDescriptorRanges; range_index++) {
                    const auto& range = table.pDescriptorRanges[range_index];

                    // use the space0 to bind resource, other space will be extend in the future.
                    if (range.RegisterSpace != 0) {
                        m_Context.m_Device.GetLogger()->warn("Use space 0 to bind resource, other space will be extend in the future.");
                        continue;
                    }

                    for (std::uint32_t i = 0; i < range.NumDescriptors; i++) {
                        auto        slot_type   = range_type_to_slot_type(range.RangeType);
                        std::size_t heap_offset = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? (curr_heap_offset[1]++) : (curr_heap_offset[0]++);

                        m_SlotInfos[slot_type].emplace(
                            range.BaseShaderRegister + i,
                            SlotInfo{
                                .binding_type   = BindingType::DescriptorTable,
                                .param_index    = param_index,
                                .offset_in_heap = heap_offset,
                            });

                        // we use emplace here so, only the smallest heap_offset will insert!
                        m_TableHeapOffset[param_index] = std::min(heap_offset, m_TableHeapOffset[param_index]);
                        m_TableMask.set(param_index);
                    }
                }
            } break;
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
                if (root_param.Constants.RegisterSpace != 0) {
                    m_Context.m_Device.GetLogger()->warn("Use space 0 to bind resource, other space will be extend in the future.");
                    continue;
                }
                m_SlotInfos[SlotType::CBV].emplace(
                    root_param.Constants.ShaderRegister,
                    SlotInfo{
                        .binding_type = BindingType::RootConstant,
                        .param_index  = param_index,
                    });
            } break;
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV: {
                if (root_param.Descriptor.RegisterSpace != 0) {
                    m_Context.m_Device.GetLogger()->warn("Use space 0 to bind resource, other space will be extend in the future.");
                    continue;
                }
                m_SlotInfos[root_parameter_type_to_slot_type(root_param.ParameterType)].emplace(
                    root_param.Descriptor.ShaderRegister,
                    SlotInfo{
                        .binding_type = BindingType::RootDescriptor,
                        .param_index  = param_index,
                    });
            } break;
        }
    }
    m_CBV_UAV_SRV_Cache.clear();
    m_Sampler_Cache.clear();
    m_CBV_UAV_SRV_Cache.resize(curr_heap_offset[0], {.ptr = 0});
    m_Sampler_Cache.resize(curr_heap_offset[1], {.ptr = 0});
}

void ResourceBinder::PushConstant(std::uint32_t slot, const std::span<const std::byte>& data) {
    const auto& slot_info = m_SlotInfos[SlotType::CBV].at(slot);
    if (slot_info.binding_type != BindingType::RootConstant) {
        m_Context.m_Device.GetLogger()->error(
            "Can push constant to slot(BindingType:{})",
            fmt::styled(magic_enum::enum_name(slot_info.binding_type), fmt::fg(fmt::color::red)));
        return;
    }
    switch (m_Context.m_Type) {
        case CommandType::Graphics:
            m_Context.m_CmdList->SetGraphicsRoot32BitConstants(slot_info.param_index, std::max(1ull, data.size() / 4), data.data(), 0);
            break;
        case CommandType::Compute:
            m_Context.m_CmdList->SetComputeRoot32BitConstants(slot_info.param_index, std::max(1ull, data.size() / 4), data.data(), 0);
            break;
        case CommandType::Copy:
            throw std::logic_error("Can push constant in copy command");
    }
}

void ResourceBinder::BindConstantBuffer(std::uint32_t slot, const GPUBuffer& buffer, std::size_t index) {
    const auto& slot_info = m_SlotInfos[SlotType::CBV].at(slot);
    switch (slot_info.binding_type) {
        case BindingType::RootConstant: {
            m_Context.m_Device.GetLogger()->error(
                "Can bind constant buffer to slot(BindingType:{})",
                fmt::styled(magic_enum::enum_name(slot_info.binding_type), fmt::fg(fmt::color::red)));
            return;
        } break;
        case BindingType::RootDescriptor: {
            auto buffer_location = static_cast<const DX12GPUBuffer&>(buffer).resource->GetGPUVirtualAddress();
            switch (m_Context.m_Type) {
                case CommandType::Graphics:
                    m_Context.m_CmdList->SetGraphicsRootConstantBufferView(slot_info.param_index, buffer_location);
                    break;
                case CommandType::Compute:
                    m_Context.m_CmdList->SetComputeRootConstantBufferView(slot_info.param_index, buffer_location);
                    break;
                case CommandType::Copy:
                    throw std::invalid_argument("Can bind constant buffer in copy command");
            }
        } break;
        case BindingType::DescriptorTable: {
            const auto& d3d_buffer = static_cast<const DX12GPUBuffer&>(buffer);
            CacheDescriptor(SlotType::CBV, d3d_buffer.cbvs, index, slot_info.offset_in_heap);
        } break;
    }
}

void ResourceBinder::BindTexture(std::uint32_t slot, const Texture& texture) {
    const auto& slot_info = m_SlotInfos[SlotType::SRV].at(slot);
    assert(slot_info.binding_type == BindingType::DescriptorTable);
    const auto& d3d_texture = static_cast<const DX12Texture&>(texture);
    CacheDescriptor(SlotType::SRV, d3d_texture.srv, 0, slot_info.offset_in_heap);
}

void ResourceBinder::FlushDescriptors() {
    if (m_TableMask.none()) return;

    if (m_CBV_UAV_SRV_Cache.empty() && m_Sampler_Cache.empty()) {
        return;
    }

    Descriptor cbv_uav_srv_heap, sampler_heap;
    if (!m_CBV_UAV_SRV_Cache.empty()) {
        cbv_uav_srv_heap = m_Context.m_Device.AllocateDynamicDescriptors(m_CBV_UAV_SRV_Cache.size(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    if (!m_Sampler_Cache.empty()) {
        sampler_heap = m_Context.m_Device.AllocateDynamicDescriptors(m_Sampler_Cache.size(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    magic_enum::enum_for_each<SlotType>([&](SlotType slot_type) {
        auto& heap  = slot_type == SlotType::Sampler ? sampler_heap : cbv_uav_srv_heap;
        auto& cache = slot_type == SlotType::Sampler ? m_Sampler_Cache : m_CBV_UAV_SRV_Cache;

        for (const auto& [slot, info] : m_SlotInfos[slot_type]) {
            if (info.binding_type != BindingType::DescriptorTable) continue;
            if (cache.at(info.offset_in_heap).ptr == 0) {
                m_Context.m_Device.GetLogger()->error(
                    "No descriptor bind to the descriptor table({}:{})!",
                    fmt::styled(info.param_index, fmt::fg(fmt::color::green)),
                    fmt::styled(magic_enum::enum_name(info.binding_type), fmt::fg(fmt::color::green)));
            }
            // clear dirty

            m_Context.m_Device.GetDevice()->CopyDescriptorsSimple(
                1,
                CD3DX12_CPU_DESCRIPTOR_HANDLE(heap.cpu_handle, info.offset_in_heap, heap.increament_size),
                cache.at(info.offset_in_heap),
                slot_type == SlotType::Sampler
                    ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
                    : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    });

    // Bind Descriptor Heap
    auto        heaps     = utils::create_array<ID3D12DescriptorHeap*, 2>(nullptr);
    std::size_t num_heaps = 0;
    if (cbv_uav_srv_heap) {
        heaps[0] = cbv_uav_srv_heap.heap_from->GetHeap();
        num_heaps++;
    }
    if (sampler_heap) {
        heaps[1] = sampler_heap.heap_from->GetHeap();
        num_heaps++;
    }
    m_Context.m_CmdList->SetDescriptorHeaps(num_heaps, heaps.data());

    // Bind Descriptor Table
    // TODO we need to know which heap will be bind, sampler or cbv_uav_srv ?
    for (std::uint8_t param_index = 0; param_index < sm_MaxRootParameters; param_index++) {
        if (!m_TableMask.test(param_index)) continue;
        auto& heap = m_CurrRootSignatureDesc->pParameters[param_index].DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
                         ? sampler_heap
                         : cbv_uav_srv_heap;

        auto base_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(heap.gpu_handle, m_TableHeapOffset[param_index], heap.increament_size);

        switch (m_Context.m_Type) {
            case CommandType::Graphics:
                m_Context.m_CmdList->SetGraphicsRootDescriptorTable(
                    param_index,
                    base_descriptor_handle);
                break;
            case CommandType::Compute:
                m_Context.m_CmdList->SetComputeRootDescriptorTable(
                    param_index,
                    base_descriptor_handle);
            case CommandType::Copy:
                throw std::logic_error("Can bind descriptor table in copy command");
        }
    }

    m_UsedHeaps.emplace_back(std::move(cbv_uav_srv_heap));
    m_UsedHeaps.emplace_back(std::move(sampler_heap));
}

void ResourceBinder::CacheDescriptor(SlotType slot_type, const Descriptor& descriptor, std::size_t descriptor_index, std::size_t heap_offset) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle(descriptor.cpu_handle, descriptor_index, descriptor.increament_size);
    switch (slot_type) {
        case SlotType::CBV:
        case SlotType::UAV:
        case SlotType::SRV: {
            // TODO: may use a unbound (Bindless) root signature
            assert(heap_offset < m_CBV_UAV_SRV_Cache.size());
            m_CBV_UAV_SRV_Cache[heap_offset] = descriptor_handle;
        } break;
        case SlotType::Sampler: {
            m_Sampler_Cache[heap_offset] = descriptor_handle;
        } break;
    }
}

}  // namespace hitagi::gfx