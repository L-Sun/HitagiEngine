#include "allocator.hpp"
#include "utils.hpp"
#include "dx12_device.hpp"

namespace hitagi::graphics::backend::DX12 {

Allocation::~Allocation() {
    if (auto page = page_from.lock())
        page->DiscardAllocation(*this);
}
AllocationPage::AllocationPage(AllocationPage&& rhs) noexcept
    : GpuResource(std::move(rhs)),
      m_Size(rhs.m_Size),
      m_CpuPtr(rhs.m_CpuPtr) {
    rhs.m_CpuPtr = nullptr;
}
AllocationPage& AllocationPage::operator=(AllocationPage&& rhs) noexcept {
    if (&rhs != this) {
        GpuResource::operator=(std::move(rhs));
        m_Size       = rhs.m_Size;
        m_CpuPtr     = rhs.m_CpuPtr;
        rhs.m_Size   = 0;
        rhs.m_CpuPtr = nullptr;
    }
    return *this;
};

std::array<LinearAllocator::PageManager, static_cast<std::size_t>(AllocationPageType::NUM_TYPES)>
    LinearAllocator::sm_PageManager = {
        PageManager(AllocationPageType::GPU_EXCLUSIVE, sm_GpuAllocatorPageSize),
        PageManager(AllocationPageType::CPU_WRITABLE, sm_CpuAllocatorPageSize),
};

Allocation LinearAllocator::Allocate(std::size_t size, std::size_t alignment) {
    std::size_t aligned_size = utils::align(size, alignment);
    auto&       mgr          = sm_PageManager[static_cast<std::size_t>(m_Type)];

    // need large page
    if (aligned_size > GetDefaultSize(m_Type)) {
        auto& page = m_LargePages.emplace_back(mgr.RequesetLargePage(m_Device, aligned_size));
        return {page, 0, aligned_size, page->m_CpuPtr, (*page)->GetGPUVirtualAddress()};
    }

    if (m_CurrPage == nullptr) {
        m_CurrPage = m_Pages.emplace_back(mgr.RequesetPage(m_Device));
    }

    m_CurrPage->m_Offset = utils::align(m_CurrPage->m_Offset, alignment);

    if (aligned_size > m_CurrPage->GetFreeSize())
        m_CurrPage = m_Pages.emplace_back(mgr.RequesetPage(m_Device));

    std::size_t               offset  = m_CurrPage->m_Offset;
    std::byte*                cpu_ptr = m_CurrPage->m_CpuPtr + m_CurrPage->m_Offset;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_ptr = m_CurrPage->m_Resource->GetGPUVirtualAddress() + m_CurrPage->m_Offset;

    m_CurrPage->m_Offset += aligned_size;
    m_CurrPage->m_allocation_count++;

    return {m_CurrPage, offset, size, cpu_ptr, gpu_ptr};
}

void LinearAllocator::SetFence(FenceValue fence) noexcept {
    // no allocation occur, just return.
    auto& mgr = sm_PageManager[static_cast<std::size_t>(m_Type)];
    for (auto&& page : m_Pages)
        mgr.DiscardPage(std::move(page), fence);
    m_Pages.clear();

    for (auto&& page : m_LargePages)
        mgr.DiscardLargePage(std::move(page), fence);
    m_LargePages.clear();
}

std::shared_ptr<LinearAllocator::LinearAllocationPage> LinearAllocator::PageManager::CreateNewPage(DX12Device* device, AllocationPageType type, std::size_t size) {
    CD3DX12_HEAP_PROPERTIES heap_prop;
    D3D12_RESOURCE_STATES   default_usage;
    CD3DX12_RESOURCE_DESC   desc;

    switch (type) {
        case AllocationPageType::GPU_EXCLUSIVE: {
            heap_prop     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            desc          = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            default_usage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        } break;
        case AllocationPageType::CPU_WRITABLE: {
            heap_prop     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            desc          = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
            default_usage = D3D12_RESOURCE_STATE_GENERIC_READ;
        } break;
        default: {
            throw std::bad_alloc();
        }
    }
    ID3D12Resource* resource = nullptr;
    ThrowIfFailed(device->GetDevice()->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &desc, default_usage, nullptr, IID_PPV_ARGS(&resource)));
    resource->SetName(L"Linear Allocation Page");
    // resource ownership transfer to GpuResource
    auto ret = std::make_shared<LinearAllocationPage>(GpuResource{device, resource}, size);
    return ret;
}

void LinearAllocator::PageManager::UpdateAvailablePages(std::function<bool(FenceValue)>&& fence_checker) {
    std::lock_guard lock(m_Mutex);

    for (auto iter = m_RetiredPages.begin(); iter != m_RetiredPages.end();) {
        auto&& [page, fence] = *iter;
        if (fence_checker(fence) && page->m_allocation_count == 0) {
            page->m_Offset = 0;
            m_AvailablePages.emplace(std::move(page));
            iter = m_RetiredPages.erase(iter);
        } else {
            iter++;
        }
    }

    for (auto iter = m_LargePages.begin(); iter != m_LargePages.end();) {
        auto&& [page, fence] = *iter;
        if (fence_checker(fence)) {
            iter = m_LargePages.erase(iter);
        } else {
            iter++;
        }
    }
}

std::shared_ptr<LinearAllocator::LinearAllocationPage> LinearAllocator::PageManager::RequesetPage(DX12Device* device) {
    {
        std::lock_guard lock(m_Mutex);
        if (!m_AvailablePages.empty()) {
            auto page = std::move(m_AvailablePages.front());
            m_AvailablePages.pop();
            return page;
        }
    }  // unlock mutex

    // Create New Page
    return CreateNewPage(device, m_PageType, m_DefaultSize);
}

void LinearAllocator::PageManager::DiscardPage(std::shared_ptr<LinearAllocationPage> page, FenceValue fence) noexcept {
    std::lock_guard lock(m_Mutex);
    m_RetiredPages.emplace_back(std::move(page), fence);
}
void LinearAllocator::PageManager::DiscardLargePage(std::shared_ptr<LinearAllocationPage> page, FenceValue fence) noexcept {
    std::lock_guard lock(m_Mutex);
    m_LargePages.emplace_back(std::move(page), fence);
}
void LinearAllocator::PageManager::Reset() {
    std::lock_guard lock(m_Mutex);
    m_RetiredPages.clear();
    m_AvailablePages = {};  // use a empty queue to clear
    m_LargePages.clear();
}
}  // namespace hitagi::graphics::backend::DX12