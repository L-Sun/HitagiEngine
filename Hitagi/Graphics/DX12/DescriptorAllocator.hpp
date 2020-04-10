#pragma once
#include "D3Dpch.hpp"

namespace Hitagi::Graphics::DX12 {

class DescriptorAllocation;
class DescriptorAllocatorPage;

class DescriptorAllocator {
public:
    void Initialize(ID3D12Device6* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescPerHeap = 256);

    DescriptorAllocation Allocate(uint32_t numDescriptor = 1);
    void                 ReleaseStaleDescriptor(uint64_t fenceValue);

private:
    using DescriptorHeapPool = std::vector<std::shared_ptr<DescriptorAllocatorPage>>;
    std::shared_ptr<DescriptorAllocatorPage> CreateAllocatorPage();

    ID3D12Device6*             m_Device;
    D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
    uint32_t                   m_NumDescriptorsPerHeap;

    DescriptorHeapPool m_HeapPool;
    // Indices of available heaps in the heap pool.
    std::set<size_t> m_AvailbaleHeaps;

    std::mutex m_AllocationMutex;
};

class DescriptorAllocation {
public:
    DescriptorAllocation();
    DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE handle, uint32_t numDescriptors, uint32_t descriptorSize,
                         std::shared_ptr<DescriptorAllocatorPage> pageFrom);

    DescriptorAllocation(const DescriptorAllocation&) = delete;
    DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

    DescriptorAllocation(DescriptorAllocation&& allocation);
    DescriptorAllocation& operator=(DescriptorAllocation&& rhs);

    ~DescriptorAllocation();

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const;

    uint32_t GetNumDescriptors() const { return m_numDescriptors; }

    operator bool() const noexcept { return m_CpuHandle.ptr != 0; }

private:
    void Free(uint64_t fenceValue);

    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_GpuHandle;
    uint32_t                    m_numDescriptors;
    uint32_t                    m_DescriptorSize;

    std::shared_ptr<DescriptorAllocatorPage> m_PageFrom;
};

class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage> {
public:
    DescriptorAllocatorPage(ID3D12Device6* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
    DescriptorAllocation Allocate(uint32_t numDescriptors);
    void                 Free(DescriptorAllocation&& descriptor, uint64_t fenceValue);
    void                 ReleaseStaleDescriptor(uint64_t fenceValue);

    inline uint32_t                   NumFreeHandles() const { return m_NumFreeHandle; }
    inline D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_Type; }
    inline bool                       HasSpace(uint32_t numDescriptors) const {
        return m_FreeListBySize.lower_bound(numDescriptors) != m_FreeListBySize.cend();
    }

protected:
    uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle);
    void     AddNewBlock(uint32_t offset, uint32_t numDescriptors);
    void     FreeBlock(uint32_t offset, uint32_t numDescriptors);

private:
    using FreeListByOffset = std::map<uint32_t, size_t>;
    using FreeListBySize   = std::multimap<uint32_t, size_t>;
    using OffsetIter       = FreeListByOffset::iterator;
    using SizeIter         = FreeListBySize::iterator;

    OffsetIter& GetOffsetIt(SizeIter& sizeIt) { return m_FreeListIter[sizeIt->second].first; }
    SizeIter&   GetSizeIt(OffsetIter& offsetIter) { return m_FreeListIter[offsetIter->second].second; }

    FreeListBySize                               m_FreeListBySize;
    FreeListByOffset                             m_FreeListByOffset;
    std::vector<std::pair<OffsetIter, SizeIter>> m_FreeListIter;
    // the available index in m_FreeListIter for the next time use of AddNewBlock
    std::queue<size_t> m_FreeIndexForIter;

    // Stale descriptors are queued for release until the frame that they were freed has finished
    // executing on the GPU.
    struct StaleDescriptorInfo {
        uint32_t offset, number;
        uint64_t fenceValue;
    };
    std::queue<StaleDescriptorInfo> m_StaleDescriptors;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_DESCRIPTOR_HEAP_TYPE                   m_Type;
    uint32_t                                     m_NumDescriptors;
    D3D12_CPU_DESCRIPTOR_HANDLE                  m_BaseHandle;
    uint32_t                                     m_HandleIncrementSize;
    uint32_t                                     m_NumFreeHandle;

    std::mutex m_AllocationMutex;
};
}  // namespace Hitagi::Graphics::DX12