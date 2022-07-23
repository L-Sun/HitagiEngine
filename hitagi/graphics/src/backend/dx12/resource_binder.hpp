#pragma once
#include "descriptor_allocator.hpp"
#include "gpu_buffer.hpp"
#include "sampler.hpp"

#include <magic_enum.hpp>

// Stage, commit  CPU visible descriptor to GPU visible descriptor.
namespace hitagi::graphics::backend::DX12 {
class CommandContext;

class ResourceBinder {
    using FenceValue   = std::uint64_t;
    using FenceChecker = std::function<bool(FenceValue)>;

    static constexpr std::size_t sm_max_root_parameters = 64;
    static constexpr std::size_t sm_heap_size           = 1024;

public:
    ResourceBinder(ID3D12Device* device, CommandContext& context, FenceChecker&& checker);

    void BindResource(std::uint32_t slot, ConstantBuffer* cb, std::size_t offset);
    void BindResource(std::uint32_t slot, TextureBuffer* tb);
    void BindResource(std::uint32_t slot, Sampler* sampler);
    void Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count);

    void CommitStagedDescriptors();

    // Paser root signature to get information about the layout of descriptors.
    void ParseRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc);

    void Reset(FenceValue fence_value);

    // It should be invoke at lest time when initialize dx12 device os that it can use our custom pmr allocator
    static void ResetHeapPool() {
        std::lock_guard lock{sm_Mutex};
        sm_DescriptorHeapPool       = {};
        sm_AvailableDescriptorHeaps = {};
    }

private:
    ComPtr<ID3D12DescriptorHeap>        RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
    static ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type);
    std::uint32_t                       StaleDescriptorCount(D3D12_DESCRIPTOR_HEAP_TYPE heap_type) const;

    void StageDescriptor(std::uint32_t heap_offset, const std::shared_ptr<Descriptor>& descriptor);
    void StageDescriptors(std::uint32_t heap_offset, const std::pmr::vector<std::shared_ptr<Descriptor>>& descriptors);

    struct DescriptorCache {
        std::shared_ptr<Descriptor> descriptor = nullptr;
        std::uint32_t               root_index = 0;
    };

    struct SlotInfo {
        bool in_table = false;
        // if type is root constant or root descriptor, the value indicates its root index
        // else it indicates the offset in the descriptor heap
        std::uint64_t value = 0;
    };

    using DescriptorHeapPool = std::queue<ComPtr<ID3D12DescriptorHeap>, std::pmr::deque<ComPtr<ID3D12DescriptorHeap>>>;
    using AvailableHeapPool  = std::queue<
        std::pair<ComPtr<ID3D12DescriptorHeap>, FenceValue>,
        std::pmr::deque<std::pair<ComPtr<ID3D12DescriptorHeap>, FenceValue>>>;

    // only cbv_srv_uav and sampler heap supported
    inline static std::array<DescriptorHeapPool, 2> sm_DescriptorHeapPool;
    inline static std::array<AvailableHeapPool, 2>  sm_AvailableDescriptorHeaps;
    static std::mutex                               sm_Mutex;

    ID3D12Device*   m_Device;
    CommandContext& m_Context;
    FenceChecker    m_FenceChecker;

    std::array<std::array<DescriptorCache, sm_heap_size>, 2> m_DescriptorCaches;

    std::array<std::pmr::unordered_map<std::uint32_t, SlotInfo>, magic_enum::enum_count<Descriptor::Type>()> m_SlotInfos;
    std::array<std::pmr::map<std::uint32_t, std::uint64_t>, 2>                                               m_RootIndexToHeapOffset;

    // Each bit indicates which root table is declarated in root signature.
    std::bitset<sm_max_root_parameters> m_RootTableMask;
    // Each bit indicates the root table in the root signature that has
    // changed since the last time the descriptors were copied.
    std::bitset<sm_max_root_parameters> m_RootTableDirty;

    std::array<ComPtr<ID3D12DescriptorHeap>, 2>  m_CurrentDescriptorHeaps;
    std::array<std::size_t, 2>                   m_HandleIncrementSizes;
    std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> m_CurrentCPUDescriptorHandles;
    std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> m_CurrentGPUDescriptorHandles;
    std::array<std::uint32_t, 2>                 m_NumFreeHandles;
};
}  // namespace hitagi::graphics::backend::DX12