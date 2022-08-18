#include "resource_binder.hpp"
#include "command_context.hpp"
#include "utils.hpp"
#include "dx12_device.hpp"

#include <hitagi/utils/utils.hpp>
#include <numeric>

#include <spdlog/spdlog.h>

namespace hitagi::graphics::backend::DX12 {

std::mutex ResourceBinder::sm_Mutex;

ResourceBinder::ResourceBinder(DX12Device* device, CommandContext& context, FenceChecker&& checker)
    : m_Device(device),
      m_Context(context),
      m_FenceChecker(std::move(checker)),
      m_CurrentDescriptorHeaps(
          utils::create_array<ID3D12DescriptorHeap*, 2>(nullptr)),
      m_CurrentCPUDescriptorHandles(
          utils::create_array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2>(D3D12_DEFAULT)),
      m_CurrentGPUDescriptorHandles(
          utils::create_array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2>(D3D12_DEFAULT)),
      m_NumFreeHandles(
          utils::create_array<std::uint32_t, 2>(0))  //
{
    m_HandleIncrementSizes[0] = m_Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_HandleIncrementSizes[1] = m_Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

void ResourceBinder::Reset(FenceValue fence_value) {
    std::lock_guard lock{sm_Mutex};

    for (std::size_t heap_index = 0; heap_index < 2; heap_index++) {
        if (m_CurrentDescriptorHeaps[heap_index] != nullptr)
            sm_RetiredDescriptorHeaps[heap_index].push({m_CurrentDescriptorHeaps[heap_index], fence_value});

        for (auto heap : m_RetiredDescriptorHeaps[heap_index])
            sm_RetiredDescriptorHeaps[heap_index].push({heap, fence_value});

        m_CurrentCPUDescriptorHandles[heap_index] = D3D12_DEFAULT;
        m_CurrentGPUDescriptorHandles[heap_index] = D3D12_DEFAULT;
        m_NumFreeHandles[heap_index]              = 0;
    }

    m_RootTableMask.reset();
    m_RootTableDirty.reset();

    // Reset the table cache
    m_DescriptorCaches.fill({});
}

void ResourceBinder::ParseRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc) {
    auto logger = spdlog::get("GraphicsManager");

    m_RootTableMask.reset();
    m_SlotInfos.fill({});
    m_RootIndexToHeapOffset.fill({});

    auto current_ofssets = utils::create_array<std::uint32_t, 2>(0);

    for (std::uint32_t root_index = 0; root_index < root_signature_desc.NumParameters; root_index++) {
        const auto& root_param = root_signature_desc.pParameters[root_index];

        switch (root_param.ParameterType) {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
                const auto& table = root_param.DescriptorTable;
                assert(table.pDescriptorRanges != nullptr);

                for (std::uint32_t range_index = 0; range_index < table.NumDescriptorRanges; range_index++) {
                    const auto& range = table.pDescriptorRanges[range_index];

                    // use the space0 to bind resource, other space will be extend in the future.
                    if (range.RegisterSpace != 0) {
                        logger->debug("Use space 0 to bind resource, other space will be extend in the future.");
                        continue;
                    }
                    m_RootTableMask.set(root_index);

                    std::size_t heap_index = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 1 : 0;

                    for (std::uint32_t i = 0; i < range.NumDescriptors; i++) {
                        auto descriptor_type = range_type_to_descriptor_type(range.RangeType);

                        std::size_t heap_offset = current_ofssets[heap_index]++;

                        m_SlotInfos[magic_enum::enum_integer(descriptor_type)].emplace(
                            range.BaseShaderRegister + i,
                            SlotInfo{
                                .in_table = true,
                                .value    = heap_offset,
                            });

                        // we use emplace here so, only the smallest heap_offset will insert!
                        m_RootIndexToHeapOffset[heap_index].emplace(root_index, heap_offset);

                        m_DescriptorCaches[heap_index].at(heap_offset) =
                            DescriptorCache{
                                .handle     = {0},
                                .root_index = root_index,
                            };
                    }
                }
            } break;
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
                if (root_param.Constants.RegisterSpace != 0) {
                    logger->debug("Use space 0 to bind resource, other space will be extend in the future.");
                    continue;
                }
                m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::CBV)].emplace(
                    root_param.Constants.ShaderRegister,
                    SlotInfo{
                        .in_table = false,
                        .value    = root_index,
                    });
            } break;
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV: {
                if (root_param.Descriptor.RegisterSpace != 0) {
                    logger->debug("Use space 0 to bind resource, other space will be extend in the future.");
                    continue;
                }
                m_SlotInfos[magic_enum::enum_integer(root_parameter_type_to_descriptor_type(root_param.ParameterType))].emplace(
                    root_param.Descriptor.ShaderRegister,
                    SlotInfo{
                        .in_table = false,
                        .value    = root_index,
                    });
            } break;
        }
    }
}

void ResourceBinder::BindResource(std::uint32_t slot, ConstantBuffer* cb, std::size_t offset) {
    if (!SlotCheck(Descriptor::Type::CBV, slot)) return;

    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::CBV)].at(slot);
    if (slot_info.in_table) {
        StageDescriptor(slot_info.value, cb->GetCBV(offset));
    } else {
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address = cb->GetResource()->GetGPUVirtualAddress() + offset * cb->GetBlockSize();
        if (m_Context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
            m_Context.GetCommandList()->SetComputeRootConstantBufferView(slot_info.value, gpu_address);
        } else {
            m_Context.GetCommandList()->SetGraphicsRootConstantBufferView(slot_info.value, gpu_address);
        }
    }
}
void ResourceBinder::BindResource(std::uint32_t slot, Texture* tb) {
    if (!SlotCheck(Descriptor::Type::SRV, slot)) return;

    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::SRV)].at(slot);
    if (slot_info.in_table) {
        StageDescriptor(slot_info.value, tb->GetSRV());
    } else {
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address = tb->GetResource()->GetGPUVirtualAddress();
        if (m_Context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
            m_Context.GetCommandList()->SetComputeRootShaderResourceView(slot_info.value, gpu_address);
        } else {
            m_Context.GetCommandList()->SetGraphicsRootShaderResourceView(slot_info.value, gpu_address);
        }
    }
}
void ResourceBinder::BindResource(std::uint32_t slot, Sampler* sampler) {
    if (!SlotCheck(Descriptor::Type::Sampler, slot)) return;

    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::Sampler)].at(slot);
    assert(slot_info.in_table);
    StageDescriptor(slot_info.value, sampler->GetDescriptor());
}

void ResourceBinder::Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count) {
    if (!SlotCheck(Descriptor::Type::CBV, slot)) return;

    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::CBV)].at(slot);
    assert(!slot_info.in_table);
    if (m_Context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
        m_Context.GetCommandList()->SetComputeRoot32BitConstants(slot_info.value, count, data, 0);
    } else {
        m_Context.GetCommandList()->SetGraphicsRoot32BitConstants(slot_info.value, count, data, 0);
    }
}

void ResourceBinder::BindDynamicTextureBuffer(std::uint32_t slot, const D3D12_GPU_VIRTUAL_ADDRESS& gpu_address) {
    if (!SlotCheck(Descriptor::Type::SRV, slot)) return;

    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::SRV)].at(slot);
    if (slot_info.in_table) {
        auto logger = spdlog::get("GraphicsManager");
        logger->error("Can not bind dynamic texture buffer to descriptor table! Only root constant buffer supported!");
        return;
    } else {
        if (m_Context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
            m_Context.GetCommandList()->SetComputeRootShaderResourceView(slot_info.value, gpu_address);
        } else {
            m_Context.GetCommandList()->SetGraphicsRootShaderResourceView(slot_info.value, gpu_address);
        }
    }
}

void ResourceBinder::BindDynamicConstantBuffer(std::uint32_t slot, const D3D12_GPU_VIRTUAL_ADDRESS& gpu_address) {
    if (!SlotCheck(Descriptor::Type::CBV, slot)) return;

    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::CBV)].at(slot);
    if (slot_info.in_table) {
        auto logger = spdlog::get("GraphicsManager");
        logger->error("Can not bind dynamic constant buffer to descriptor table! Only root constant buffer supported!");
        return;
    } else {
        if (m_Context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
            m_Context.GetCommandList()->SetComputeRootConstantBufferView(slot_info.value, gpu_address);
        } else {
            m_Context.GetCommandList()->SetGraphicsRootConstantBufferView(slot_info.value, gpu_address);
        }
    }
}

void ResourceBinder::StageDescriptor(std::uint32_t offset, const Descriptor& descriptor) {
    StageDescriptors(offset, {std::cref(descriptor)});
}

void ResourceBinder::StageDescriptors(std::uint32_t offset, const std::pmr::vector<std::reference_wrapper<const Descriptor>>& descriptors) {
    if (descriptors.empty()) return;
    assert(offset + descriptors.size() <= sm_heap_size);

    auto iter = std::adjacent_find(descriptors.begin(), descriptors.end(), [](const auto& a, const auto& b) -> bool {
        return a.get().type == b.get().type;
    });

    if (iter != descriptors.end()) {
        std::size_t index = std::distance(iter, descriptors.end());
        throw std::invalid_argument(fmt::format(
            "Expect all descriptor have same type, but its type ({}) is diffrent from ({}) at index {}",
            magic_enum::enum_name(descriptors[index].get().type),
            magic_enum::enum_name(descriptors[index - 1].get().type),
            index));
    }
    auto descriptor_type = descriptors.front().get().type;
    assert(descriptor_type != Descriptor::Type::DSV && descriptor_type != Descriptor::Type::RTV);

    std::size_t heap_index = descriptor_type == Descriptor::Type::Sampler ? 1 : 0;

    m_StaleDescriptorCount[heap_index] += descriptors.size();
    for (std::size_t i = 0; i < descriptors.size(); i++) {
        DescriptorCache& cache = m_DescriptorCaches[heap_index].at(offset + i);

        if (cache.handle.ptr == descriptors[i].get().handle.ptr) continue;

        cache.handle = descriptors[i].get().handle;
        m_RootTableDirty.set(cache.root_index);
    }
}

ID3D12DescriptorHeap* ResourceBinder::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heap_type) {
    std::lock_guard lock(sm_Mutex);

    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    auto&                        available_heap_pool = sm_AvailableDescriptorHeaps[heap_type];

    while (!sm_RetiredDescriptorHeaps[heap_type].empty() && m_FenceChecker(sm_RetiredDescriptorHeaps[heap_type].front().second)) {
        sm_AvailableDescriptorHeaps[heap_type].push(sm_RetiredDescriptorHeaps[heap_type].front().first);
        sm_RetiredDescriptorHeaps[heap_type].pop();
    }

    if (!available_heap_pool.empty()) {
        descriptor_heap = available_heap_pool.front();
        available_heap_pool.pop();
    } else {
        descriptor_heap = CreateDescriptorHeap(m_Device, heap_type);
        sm_DescriptorHeapPool[heap_type].push(descriptor_heap);
    }

    return descriptor_heap.Get();
}

ComPtr<ID3D12DescriptorHeap> ResourceBinder::CreateDescriptorHeap(
    DX12Device*                device,
    D3D12_DESCRIPTOR_HEAP_TYPE type) {
    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
    descriptor_heap_desc.Type                       = type;
    descriptor_heap_desc.NumDescriptors             = sm_heap_size;
    descriptor_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    ThrowIfFailed(device->GetDevice()->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));

    return descriptor_heap;
}

void ResourceBinder::CommitStagedDescriptors() {
    D3D12_COMMAND_LIST_TYPE cmd_type = m_Context.GetType();
    auto                    cmd_list = m_Context.GetCommandList();

    for (int heap_index = 0; heap_index < 2; heap_index++) {
        D3D12_DESCRIPTOR_HEAP_TYPE heap_type = heap_index == 0 ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

        if (m_StaleDescriptorCount[heap_type] == 0) continue;

        if (m_NumFreeHandles[heap_index] < m_StaleDescriptorCount[heap_type]) {
            if (m_CurrentDescriptorHeaps[heap_index] != nullptr)
                m_RetiredDescriptorHeaps[heap_index].emplace_back(m_CurrentDescriptorHeaps[heap_index]);

            m_CurrentDescriptorHeaps[heap_index]      = RequestDescriptorHeap(heap_type);
            m_CurrentCPUDescriptorHandles[heap_index] = m_CurrentDescriptorHeaps[heap_index]->GetCPUDescriptorHandleForHeapStart();
            m_CurrentGPUDescriptorHandles[heap_index] = m_CurrentDescriptorHeaps[heap_index]->GetGPUDescriptorHandleForHeapStart();
            m_NumFreeHandles[heap_index]              = sm_heap_size;

            m_Context.SetDescriptorHeap(heap_type, m_CurrentDescriptorHeaps[heap_index]);
            // We need to update whole new descriptor heap
            m_RootTableDirty = m_RootTableMask;
        }

        // calcuate continous range
        std::size_t                                                     num_src_descriptor_ranges = 0;
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, sm_max_root_parameters> src_descriptor_range_starts;
        std::array<UINT, sm_max_root_parameters>                        src_descriptor_range_sizes;

        std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, std::uint32_t> range = {{}, 0};
        for (const auto& cache : m_DescriptorCaches[heap_type]) {
            if (cache.handle.ptr == 0) break;
            if (!m_RootTableDirty.test(cache.root_index)) continue;

            if (cache.handle.ptr == range.first.ptr + range.second * m_HandleIncrementSizes[heap_type]) {
                range.second++;
            } else {
                if (range.second != 0) {
                    src_descriptor_range_starts[num_src_descriptor_ranges].ptr = range.first.ptr;
                    src_descriptor_range_sizes[num_src_descriptor_ranges]      = range.second;
                    num_src_descriptor_ranges++;
                }
                range.first  = cache.handle;
                range.second = 1;
            }
        }
        if (range.second != 0) {
            src_descriptor_range_starts[num_src_descriptor_ranges].ptr = range.first.ptr;
            src_descriptor_range_sizes[num_src_descriptor_ranges]      = range.second;
            num_src_descriptor_ranges++;
        }
        UINT descriptor_count = std::accumulate(src_descriptor_range_sizes.begin(), std::next(src_descriptor_range_sizes.begin(), num_src_descriptor_ranges), 0);

        m_Device->GetDevice()->CopyDescriptors(
            1,
            &m_CurrentCPUDescriptorHandles[heap_type],
            &descriptor_count,
            num_src_descriptor_ranges,
            src_descriptor_range_starts.data(),
            src_descriptor_range_sizes.data(),
            heap_type);

        for (auto [root_index, heap_offset] : m_RootIndexToHeapOffset[heap_type]) {
            if (!m_RootTableDirty.test(root_index)) continue;

            if (cmd_type == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
                cmd_list->SetComputeRootDescriptorTable(
                    root_index,
                    CD3DX12_GPU_DESCRIPTOR_HANDLE(
                        m_CurrentGPUDescriptorHandles[heap_type],
                        heap_offset,
                        m_HandleIncrementSizes[heap_type]));
            } else {
                cmd_list->SetGraphicsRootDescriptorTable(
                    root_index,
                    CD3DX12_GPU_DESCRIPTOR_HANDLE(
                        m_CurrentGPUDescriptorHandles[heap_type],
                        heap_offset,
                        m_HandleIncrementSizes[heap_type]));
            }
        }

        m_CurrentCPUDescriptorHandles[heap_type].Offset(descriptor_count, m_HandleIncrementSizes[heap_type]);
        m_CurrentGPUDescriptorHandles[heap_type].Offset(descriptor_count, m_HandleIncrementSizes[heap_type]);
        m_NumFreeHandles[heap_type] -= descriptor_count;
    }

    m_RootTableDirty.reset();
    m_StaleDescriptorCount.fill(0);
}

bool ResourceBinder::SlotCheck(Descriptor::Type type, std::uint32_t slot) {
    if (!m_SlotInfos[magic_enum::enum_integer(type)].contains(slot)) {
        auto logger = spdlog::get("GraphicsManager");
        logger->error("Can not bind ({}) to unexisted slot ({})", magic_enum::enum_name(type), slot);
        return false;
    }
    return true;
}

}  // namespace hitagi::graphics::backend::DX12