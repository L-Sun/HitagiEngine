#include "resource_binder.hpp"
#include "magic_enum.hpp"
#include "utils.hpp"
#include "command_context.hpp"

#include <hitagi/utils/utils.hpp>

#include <spdlog/spdlog.h>

#include <d3d12.h>
#include <optional>

namespace hitagi::graphics::backend::DX12 {

std::mutex ResourceBinder::sm_Mutex;

ResourceBinder::ResourceBinder(ID3D12Device* device, FenceChecker&& checker)
    : m_Device(device),
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

    m_RootParameterMask.reset();
    m_RootParameterDirty.reset();

    // Reset the table cache
    for (auto& caches : m_DescriptorCaches) {
        for (auto& cache : caches) {
            cache.descriptor = {};
        }
    }
}

void ResourceBinder::ParseRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc) {
    auto logger = spdlog::get("GraphicsManager");

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
                    m_RootParameterMask.set(root_index);

                    std::size_t heap_index = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 1 : 0;

                    for (std::uint32_t i = 0; i < range.NumDescriptors; i++) {
                        auto descriptor_type = range_type_to_descriptor_type(range.RangeType);

                        std::size_t heap_offset = current_ofssets[heap_index]++;

                        m_SlotToHeapOffset[magic_enum::enum_integer(descriptor_type)].emplace(range.BaseShaderRegister + i, heap_offset);
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
            default:
                // since RootDescriptor only need the resource gpu address, and 32bit constant need cpu address,
                // there are not descriptor need to be staged.
                break;
        }
    }
}
void ResourceBinder::StageDescriptor(std::uint32_t slot, const std::shared_ptr<Descriptor>& descriptor) {
    assert(descriptor != nullptr);

    auto descriptor_type = descriptor->type;
    assert(descriptor_type != Descriptor::Type::DSV && descriptor_type != Descriptor::Type::RTV);

    std::size_t heap_index = descriptor->type == Descriptor::Type::Sampler ? 1 : 0;

    std::uint32_t    heap_offset = m_SlotToHeapOffset[magic_enum::enum_integer(descriptor->type)].at(slot);
    DescriptorCache& cache       = m_DescriptorCaches[heap_index].at(heap_offset);

    cache.descriptor = descriptor;
    m_RootParameterDirty.set(cache.root_index);
}

void ResourceBinder::StageDescriptors(std::uint32_t slot, const std::pmr::vector<std::shared_ptr<Descriptor>>& descriptors) {
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

    for (std::size_t i = 0; i < descriptors.size(); i++) {
        StageDescriptor(slot + i, descriptors[i]);
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

void ResourceBinder::CommitStagedDescriptors(CommandContext& context) {
    auto cmd_list = context.GetCommandList();
    assert(cmd_list != nullptr);

    for (int heap_index = 0; heap_index < 2; heap_index++) {
        D3D12_DESCRIPTOR_HEAP_TYPE heap_type = heap_index == 0 ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

        if (m_NumFreeHandles[heap_index] < StaleDescriptorCount(heap_type)) {
            m_CurrentDescriptorHeaps[heap_index]      = RequestDescriptorHeap(heap_type);
            m_CurrentCPUDescriptorHandles[heap_index] = m_CurrentDescriptorHeaps[heap_index]->GetCPUDescriptorHandleForHeapStart();
            m_CurrentGPUDescriptorHandles[heap_index] = m_CurrentDescriptorHeaps[heap_index]->GetGPUDescriptorHandleForHeapStart();
            m_NumFreeHandles[heap_index]              = sm_heap_size;

            context.SetDescriptorHeap(heap_type, m_CurrentDescriptorHeaps[heap_index].Get());
            // We need to update whole new descriptor heap
            m_RootParameterDirty = m_RootParameterMask;
        }

        // calcuate continous range
        std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, std::uint32_t> range = {{}, 0};
        std::pmr::vector<decltype(range)>                       ranges;
        for (const auto& cache : m_DescriptorCaches[heap_type]) {
            if (cache.descriptor == nullptr) break;
            if (!m_RootParameterDirty.test(cache.root_index)) continue;

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
            if (!m_RootParameterDirty.test(root_index)) continue;

            if (context.GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
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
    m_RootParameterDirty.reset();
}

}  // namespace hitagi::graphics::backend::DX12