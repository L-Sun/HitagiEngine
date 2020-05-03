#pragma once
#include "GpuBuffer.hpp"

namespace Hitagi::Graphics::DX12 {
struct LinearAllocation {
    GpuResource&              resource;
    size_t                    offset;
    size_t                    size;
    void*                     CpuPtr;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
};

struct LinearAllocationPage : public GpuResource {
    LinearAllocationPage();
    ~LinearAllocationPage();

    size_t                    m_Offset;
    void*                     m_CpuPtr;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
};

enum LinearAllocatorType {
    kInvalidAllocator = -1,
    kGpuExclusive     = 0,  // Default
    kCpuWritable      = 1,  // cpu writable
    kNumAllocatorTypes,
};

class LinearAllocatorPageManager {
public:
    LinearAllocatorPageManager();
    LinearAllocationPage* RequestPage(void);
    LinearAllocationPage* CreateNewPage(size_t PageSize = 0);

    // Discarded pages will get recycled.  This is for fixed size pages.
    void DiscardPages(uint64_t fenceValue, const std::vector<LinearAllocationPage*>& Pages);
    void FreeLargePages(uint64_t fenceValue, const std::vector<LinearAllocationPage*>& Pages);

    void Destroy() { m_PagePool.clear(); }

private:
    static LinearAllocatorType sm_AutoType;

    LinearAllocatorType                                    m_AllocationType;
    std::vector<std::unique_ptr<LinearAllocationPage>>     m_PagePool;
    std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_RetiredPages;
    std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_DeletionQueue;
    std::queue<LinearAllocationPage*>                      m_AvailablePages;
    std::mutex                                             m_Mutex;
};

class LinearAllocator {
public:
    LinearAllocator() : m_Device(nullptr), m_PageSize(0), m_Initialized(false) {}
    LinearAllocator(ID3D12Device6* device, size_t pageSize = 2_MB)
        : m_Device(device), m_PageSize(pageSize), m_Initialized(true) {}
    ~LinearAllocator();

    void             Initalize(ID3D12Device6* device, size_t pageSize = 2_MB);
    size_t           GetPageSize() const { return m_PageSize; };
    LinearAllocation Allocate(size_t size, size_t alignment = 256);
    template <typename T>
    LinearAllocation Allocate(size_t count, size_t alignment = 256) {
        size_t size = (sizeof(T) + alignment - 1) & ~(alignment - 1);
        return Allocate(size * count, alignment);
    }
    void Reset();

private:
    LinearAllocation AllocateLargePage(size_t size);

    static LinearAllocatorPageManager sm_PageManager[kNumAllocatorTypes];

    bool                                  m_Initialized = false;
    ID3D12Device6*                        m_Device;
    std::shared_ptr<LinearAllocationPage> RequestPage();

    size_t m_PageSize;

    std::shared_ptr<LinearAllocationPage> m_CurrPage;
    size_t                                m_CurrOffset = 0;
};
}  // namespace Hitagi::Graphics::DX12