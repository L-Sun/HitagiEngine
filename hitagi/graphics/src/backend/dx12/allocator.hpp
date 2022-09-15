#pragma once
#include "gpu_resource.hpp"
#include "command_list_manager.hpp"

namespace hitagi::gfx::backend::DX12 {
class AllocationPage;
class DX12Device;

struct Allocation {
    Allocation(std::weak_ptr<AllocationPage> page_from,
               std::size_t page_offset, std::size_t size,
               std::byte*                cpu_ptr,
               D3D12_GPU_VIRTUAL_ADDRESS gpu_addr)
        : page_from(std::move(page_from)),
          page_offset(page_offset),
          size(size),
          cpu_ptr(cpu_ptr),
          gpu_addr(gpu_addr) {}

    Allocation(const Allocation&)            = delete;
    Allocation& operator=(const Allocation&) = delete;

    ~Allocation();

    std::weak_ptr<AllocationPage> page_from;
    std::size_t                   page_offset;
    std::size_t                   size;
    std::byte*                    cpu_ptr;
    D3D12_GPU_VIRTUAL_ADDRESS     gpu_addr;
};
class AllocationPage : public GpuResource {
public:
    AllocationPage(const AllocationPage&)            = delete;
    AllocationPage& operator=(const AllocationPage&) = delete;
    AllocationPage(AllocationPage&&) noexcept;
    AllocationPage& operator=(AllocationPage&&) noexcept;

    ~AllocationPage() override {
        if (m_CpuPtr) m_Resource->Unmap(0, nullptr);
    }
    // when a allocation from this page is destructing, that will call this function.
    virtual void DiscardAllocation(Allocation& allocation) = 0;
    // check is all the allocation from this page is destructed.
    virtual bool IsPageFree() const = 0;

protected:
    AllocationPage(GpuResource resource, size_t size) : GpuResource(std::move(resource)), m_Size(size) {
        m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_CpuPtr));
    }
    size_t     m_Size;
    std::byte* m_CpuPtr = nullptr;
};

enum struct AllocationPageType {
    GPU_EXCLUSIVE = 0,  // Default
    CPU_WRITABLE  = 1,  // cpu writable
    NUM_TYPES
};

// -----------------------------------
// Linear Allocator
// -----------------------------------

class LinearAllocator {
    using FenceValue   = uint64_t;
    using FenceChecker = std::function<bool(FenceValue)>;

    static constexpr size_t sm_GpuAllocatorPageSize = 0x10000;   // 64K
    static constexpr size_t sm_CpuAllocatorPageSize = 0x200000;  // 2MB

    static size_t GetDefaultSize(AllocationPageType type) {
        if (type == AllocationPageType::GPU_EXCLUSIVE) return sm_GpuAllocatorPageSize;
        if (type == AllocationPageType::CPU_WRITABLE)
            return sm_CpuAllocatorPageSize;
        else
            throw std::runtime_error("Unkown page type.");
        return 0;
    }

public:
    LinearAllocator(DX12Device* device, AllocationPageType type, FenceChecker&& fence_checker)
        : m_Device(device), m_Type(type), m_FenceChecker(std::move(fence_checker)) {
        sm_PageManager[static_cast<size_t>(m_Type)].UpdateAvailablePages(std::forward<FenceChecker>(m_FenceChecker));
    }
    LinearAllocator(const LinearAllocator&)            = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&)                 = default;
    LinearAllocator& operator=(LinearAllocator&&)      = default;

    ~LinearAllocator() {
        m_CurrPage = nullptr;
        // directly discard all unused or no effective page that is get after SetFence().
        SetFence(0);
    }

    Allocation Allocate(size_t size, size_t alignment = 256);

    // Discard all page already in effect with fence value
    void SetFence(FenceValue fence) noexcept;

    static void Destroy() {
        for (auto&& page_manager : sm_PageManager)
            page_manager.Reset();
    }

private:
    class LinearAllocationPage : public AllocationPage {
        friend LinearAllocator;

    public:
        LinearAllocationPage(GpuResource&& resource, std::size_t size) : AllocationPage(std::move(resource), size) {}
        LinearAllocationPage(LinearAllocationPage&&)            = default;
        LinearAllocationPage& operator=(LinearAllocationPage&&) = default;

        void DiscardAllocation(Allocation&) final {
            if (m_allocation_count != 0)
                m_allocation_count--;
        }

        bool IsPageFree() const noexcept final { return m_allocation_count == 0; }

        std::size_t m_Offset           = 0;
        std::size_t m_allocation_count = 0;
    };

    // Move semantic page manager
    class PageManager {
        friend LinearAllocator;

    private:
        PageManager(AllocationPageType type, std::size_t default_page_size)
            : m_PageType(type), m_DefaultSize(default_page_size) {}

        static std::shared_ptr<LinearAllocationPage> CreateNewPage(DX12Device* device, AllocationPageType type, std::size_t size);

        void UpdateAvailablePages(std::function<bool(FenceValue)>&& fence_checker);

        // page ownership will transfer to caller
        std::shared_ptr<LinearAllocationPage> RequesetPage(DX12Device* device);
        std::shared_ptr<LinearAllocationPage> RequesetLargePage(DX12Device* device, std::size_t size) {
            assert(size > m_DefaultSize);
            return CreateNewPage(device, m_PageType, size);
        }

        // get page ownership from caller
        void DiscardPage(std::shared_ptr<LinearAllocationPage> page, FenceValue fence) noexcept;
        void DiscardLargePage(std::shared_ptr<LinearAllocationPage> page, FenceValue fence) noexcept;
        void Reset();

        AllocationPageType m_PageType;
        std::size_t        m_DefaultSize;

        std::list<std::pair<std::shared_ptr<LinearAllocationPage>, FenceValue>> m_RetiredPages;
        std::queue<std::shared_ptr<LinearAllocationPage>>                       m_AvailablePages;

        // when fence is completed, the large page will delete
        std::list<std::pair<std::shared_ptr<LinearAllocationPage>, FenceValue>> m_LargePages;

        std::mutex m_Mutex;
    };

    static std::array<PageManager, static_cast<size_t>(AllocationPageType::NUM_TYPES)> sm_PageManager;

    DX12Device*                                             m_Device = nullptr;
    AllocationPageType                                      m_Type;
    std::shared_ptr<LinearAllocationPage>                   m_CurrPage = nullptr;
    std::pmr::vector<std::shared_ptr<LinearAllocationPage>> m_Pages;
    std::pmr::vector<std::shared_ptr<LinearAllocationPage>> m_LargePages;
    FenceChecker                                            m_FenceChecker;
};

}  // namespace hitagi::gfx::backend::DX12