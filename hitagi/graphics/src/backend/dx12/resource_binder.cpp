#include "resource_binder.hpp"
#include "command_context.hpp"
#include "utils.hpp"

#include <hitagi/utils/utils.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::graphics::backend::DX12 {

std::mutex ResourceBinder::sm_Mutex;

ResourceBinder::ResourceBinder(ID3D12Device* device, CommandContext& context, FenceChecker&& checker)
    : m_Device(device),
      m_Context(context),
      m_FenceChecker(std::move(checker)),
      m_CurrentDescriptorHeaps(
          utils::create_array<ComPtr<ID3D12DescriptorHeap>, 2>(nullptr)),
      m_CurrentCPUDescriptorHandles(
          utils::create_array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2>(D3D12_DEFAULT)),
      m_CurrentGPUDescriptorHandles(
          utils::create_array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2>(D3D12_DEFAULT)),
      m_NumFreeHandles(
          utils::create_array<std::uint32_t, 2>(0))  //
{
    m_HandleIncrementSizes[0] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_HandleIncrementSizes[1] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

void ResourceBinder::Reset(FenceValue fence_value) {
    for (std::size_t i = 0; i < 2; i++) {
        if (m_CurrentDescriptorHeaps[i] != nullptr)
            sm_AvailableDescriptorHeaps[i].push({m_CurrentDescriptorHeaps[i], fence_value});

        m_CurrentDescriptorHeaps[i].Reset();
        m_CurrentCPUDescriptorHandles[i] = D3D12_DEFAULT;
        m_CurrentGPUDescriptorHandles[i] = D3D12_DEFAULT;
        m_NumFreeHandles[i]              = 0;
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
                                .descriptor = nullptr,
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
void ResourceBinder::BindResource(std::uint32_t slot, TextureBuffer* tb) {
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
    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::Sampler)].at(slot);
    assert(slot_info.in_table);
    StageDescriptor(slot_info.value, sampler->GetDescriptor());
}

void ResourceBinder::Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count) {
    auto slot_info = m_SlotInfos[magic_enum::enum_integer(Descriptor::Type::CBV)].at(slot);
    assert(!slot_info.in_table);
    if (m_Context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
        m_Context.GetCommandList()->SetComputeRoot32BitConstants(slot_info.value, count, data, 0);
    } else {
        m_Context.GetCommandList()->SetGraphicsRoot32BitConstants(slot_info.value, count, data, 0);
    }
}

void ResourceBinder::StageDescriptor(std::uint32_t offset, const std::shared_ptr<Descriptor>& descriptor) {
    StageDescriptors(offset, {descriptor});
}

void ResourceBinder::StageDescriptors(std::uint32_t offset, const std::pmr::vector<std::shared_ptr<Descriptor>>& descriptors) {
    if (descriptors.empty()) return;
    assert(offset + descriptors.size() <= sm_heap_size);

    auto iter = std::adjacent_find(descriptors.begin(), descriptors.end(), [](const auto& a, const auto& b) -> bool {
        return a->type == b->type;
    });

    if (iter != descriptors.end()) {
        std::size_t index = std::distance(iter, descriptors.end());
        throw std::invalid_argument(fmt::format(
            "Expect all descriptor have same type, but its type ({}) is diffrent from ({}) at index {}",
            magic_enum::enum_name(descriptors[index]->type),
            magic_enum::enum_name(descriptors[index - 1]->type),
            index));
    }
    auto descriptor_type = descriptors.front()->type;
    assert(descriptor_type != Descriptor::Type::DSV && descriptor_type != Descriptor::Type::RTV);

    std::size_t heap_index = descriptor_type == Descriptor::Type::Sampler ? 1 : 0;

    for (std::size_t i = 0; i < descriptors.size(); i++) {
        DescriptorCache& cache = m_DescriptorCaches[heap_index].at(offset + i);
        cache.descriptor       = descriptors[i];
        m_RootTableDirty.set(cache.root_index);
    }
}

ComPtr<ID3D12DescriptorHeap> ResourceBinder::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heap_type) {
    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    auto&                        available_heap_pool = sm_AvailableDescriptorHeaps[heap_type];

    std::lock_guard lock(sm_Mutex);
    if (!available_heap_pool.empty() && m_FenceChecker(available_heap_pool.front().second)) {
        descriptor_heap = available_heap_pool.front().first;
        available_heap_pool.pop();
    } else {
        descriptor_heap = CreateDescriptorHeap(m_Device, heap_type);
        sm_DescriptorHeapPool[heap_type].push(descriptor_heap);
    }

    return descriptor_heap;
}

ComPtr<ID3D12DescriptorHeap> ResourceBinder::CreateDescriptorHeap(
    ID3D12Device*              device,
    D3D12_DESCRIPTOR_HEAP_TYPE type) {
    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
    descriptor_heap_desc.Type                       = type;
    descriptor_heap_desc.NumDescriptors             = sm_heap_size;
    descriptor_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    ThrowIfFailed(device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));

    return descriptor_heap;
}

std::uint32_t ResourceBinder::StaleDescriptorCount(D3D12_DESCRIPTOR_HEAP_TYPE heap_type) const {
    std::uint32_t result = 0;
    for (const auto& cache : m_DescriptorCaches[heap_type]) {
        if (cache.descriptor != nullptr) {
            result++;
        }
    }
    return result;
}

void ResourceBinder::CommitStagedDescriptors() {
    D3D12_COMMAND_LIST_TYPE cmd_type = m_Context.GetType();
    auto                    cmd_list = m_Context.GetCommandList();

    for (int heap_index = 0; heap_index < 2; heap_index++) {
        D3D12_DESCRIPTOR_HEAP_TYPE heap_type = heap_index == 0 ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

        if (m_NumFreeHandles[heap_index] < StaleDescriptorCount(heap_type)) {
            m_CurrentDescriptorHeaps[heap_index]      = RequestDescriptorHeap(heap_type);
            m_CurrentCPUDescriptorHandles[heap_index] = m_CurrentDescriptorHeaps[heap_index]->GetCPUDescriptorHandleForHeapStart();
            m_CurrentGPUDescriptorHandles[heap_index] = m_CurrentDescriptorHeaps[heap_index]->GetGPUDescriptorHandleForHeapStart();
            m_NumFreeHandles[heap_index]              = sm_heap_size;

            m_Context.SetDescriptorHeap(heap_type, m_CurrentDescriptorHeaps[heap_index].Get());
            // We need to update whole new descriptor heap
            m_RootTableDirty = m_RootTableMask;
        }

        // calcuate continous range
        std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, std::uint32_t> range = {{}, 0};
        std::pmr::vector<decltype(range)>                       ranges;
        for (const auto& cache : m_DescriptorCaches[heap_type]) {
            if (cache.descriptor == nullptr) break;
            if (!m_RootTableDirty.test(cache.root_index)) continue;

            if (cache.descriptor->handle.ptr == range.first.ptr + range.second * m_HandleIncrementSizes[heap_type]) {
                range.second++;
            } else {
                if (range.second != 0) {
                    ranges.emplace_back(range);
                }
                range.first  = cache.descriptor->handle;
                range.second = 1;
            }
        }
        if (range.second != 0) {
            ranges.emplace_back(range);
        }

        auto heap_gpu_handle = m_CurrentGPUDescriptorHandles[heap_type];
        for (const auto& [cpu_handle, num_descriptors] : ranges) {
            m_Device->CopyDescriptorsSimple(
                num_descriptors,
                m_CurrentCPUDescriptorHandles[heap_type],
                cpu_handle,
                heap_type);

            m_CurrentCPUDescriptorHandles[heap_type].Offset(num_descriptors, m_HandleIncrementSizes[heap_type]);
            m_CurrentGPUDescriptorHandles[heap_type].Offset(num_descriptors, m_HandleIncrementSizes[heap_type]);
            m_NumFreeHandles[heap_type] -= num_descriptors;
        }

        for (auto [root_index, heap_offset] : m_RootIndexToHeapOffset[heap_type]) {
            if (!m_RootTableDirty.test(root_index)) continue;

            if (cmd_type == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
                cmd_list->SetComputeRootDescriptorTable(
                    root_index,
                    CD3DX12_GPU_DESCRIPTOR_HANDLE(
                        heap_gpu_handle,
                        heap_offset,
                        m_HandleIncrementSizes[heap_type]));
            } else {
                cmd_list->SetGraphicsRootDescriptorTable(
                    root_index,
                    CD3DX12_GPU_DESCRIPTOR_HANDLE(
                        heap_gpu_handle,
                        heap_offset,
                        m_HandleIncrementSizes[heap_type]));
            }
        }
    }

    m_RootTableDirty.reset();
    m_DescriptorCaches.fill({});
}

}  // namespace hitagi::graphics::backend::DX12