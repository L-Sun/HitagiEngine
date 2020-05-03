#pragma once
#include "GpuBuffer.hpp"

namespace Hitagi::Graphics::DX12 {
struct LinearAllocation {
    GpuResource&              resource;
    size_t                    offset;
    size_t                    size;
    void*                     CpuPtr;
    D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress;
};

struct LinearAllocationPage : public GpuResource {
    LinearAllocationPage(ID3D12Resource* resource, D3D12_RESOURCE_STATES usage) : GpuResource() {
        m_Resource.Attach(resource);
        m_UsageState        = usage;
        m_GpuVirtualAddress = m_Resource->GetGPUVirtualAddress();
        m_Resource->Map(0, nullptr, &m_CpuPtr);
    }
    ~LinearAllocationPage() {
        if (m_CpuPtr) {
            m_Resource->Unmap(0, nullptr);
            m_CpuPtr = nullptr;
        }
    }

    size_t                    m_Offset;
    void*                     m_CpuPtr;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
};

enum class LinearAllocatorType {
    INVAILD       = -1,
    GPU_EXCLUSIVE = 0,  // Default
    CPU_WRITABLE  = 1,  // cpu writable
    NUM_TYPES,
};

enum {
    GPU_ALLOCATOR_PAGE_SIZE = 0x10000,  // 64K
    CPU_ALLOCATOR_PAGE_SIZE = 0x200000  // 2MB
};

class LinearAllocatorPageManager {
public:
    LinearAllocatorPageManager(LinearAllocatorType type);
    LinearAllocationPage*                    RequestPage();
    std::pair<size_t, LinearAllocationPage*> RequestLargePage(size_t pageSize);

    // Discarded pages will get recycled.  This is for fixed size pages.
    void DiscardPages(uint64_t fenceValue, const std::vector<LinearAllocationPage*>& pages);
    void FreeLargePages(uint64_t fenceValue, const std::vector<size_t>& largePageIndex);

    void Destroy() { m_PagePool.clear(); }

private:
    std::unique_ptr<LinearAllocationPage> CreateNewPage(size_t pageSize);

    LinearAllocatorType                                    m_Type;
    std::vector<std::unique_ptr<LinearAllocationPage>>     m_PagePool;
    std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_RetiredPages;
    std::queue<LinearAllocationPage*>                      m_AvailablePages;

    std::vector<std::unique_ptr<LinearAllocationPage>> m_LargePages;
    std::stack<size_t>                                 m_LargePagesEmptyIndex;
    std::queue<std::pair<uint64_t, size_t>>            m_DeletionQueue;  // free large pages

    std::mutex m_Mutex;
};

class LinearAllocator {
public:
    LinearAllocator(LinearAllocatorType type)
        : m_Type(type),
          m_PageSize(type == LinearAllocatorType::GPU_EXCLUSIVE ? GPU_ALLOCATOR_PAGE_SIZE : CPU_ALLOCATOR_PAGE_SIZE),
          m_CurrPage(nullptr),
          m_CurrOffset(0) {}

    size_t           GetPageSize() const { return m_PageSize; };
    LinearAllocation Allocate(size_t size, size_t alignment = 256);
    template <typename T>
    LinearAllocation Allocate(size_t count, size_t alignment = 256) {
        size_t size = (sizeof(T) + alignment - 1) & ~(alignment - 1);
        return Allocate(size * count, alignment);
    }

    void DiscardPages(uint64_t fenceValue);

    static void DestroyAll() {
        sm_PageManager[0].Destroy();
        sm_PageManager[1].Destroy();
    }

private:
    LinearAllocation AllocateLargePage(size_t size);

    static std::array<LinearAllocatorPageManager, static_cast<int>(LinearAllocatorType::NUM_TYPES)> sm_PageManager;

    LinearAllocatorType   m_Type;
    size_t                m_PageSize;
    LinearAllocationPage* m_CurrPage;
    size_t                m_CurrOffset;

    std::vector<LinearAllocationPage*> m_RetiredPages;
    std::vector<size_t>                m_LargePageIndex;
};
}  // namespace Hitagi::Graphics::DX12