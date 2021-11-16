#include "Allocator.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Graphics::backend::DX12 {

Allocation::~Allocation() {
    if (auto page = pageFrom.lock())
        page->DiscardAllocation(*this);
}
AllocationPage::AllocationPage(AllocationPage&& rhs)
    : GpuResource(std::move(rhs)),
      m_Size(rhs.m_Size),
      m_CpuPtr(rhs.m_CpuPtr) {
    rhs.m_CpuPtr = nullptr;
}
AllocationPage& AllocationPage::operator=(AllocationPage&& rhs) {
    if (&rhs != this) {
        GpuResource::operator=(std::move(rhs));
        m_Size               = rhs.m_Size;
        m_CpuPtr             = rhs.m_CpuPtr;
        rhs.m_Size           = 0;
        rhs.m_CpuPtr         = nullptr;
    }
    return *this;
};

std::array<LinearAllocator::PageManager, static_cast<size_t>(AllocationPageType::NUM_TYPES)>
    LinearAllocator::sm_PageManager = {
        PageManager(AllocationPageType::GPU_EXCLUSIVE, GPU_ALLOCATOR_PAGE_SIZE),
        PageManager(AllocationPageType::CPU_WRITABLE, CPU_ALLOCATOR_PAGE_SIZE),
};

Allocation LinearAllocator::Allocate(size_t size, size_t alignment) {
    size_t alignedSize = align(size, alignment);
    auto&  mgr         = sm_PageManager[static_cast<size_t>(m_Type)];

    // need large page
    if (alignedSize > GetDefaultSize(m_Type)) {
        auto& page = m_LargePages.emplace_back(mgr.RequesetLargePage(m_Device, alignedSize));
        return {page, 0, alignedSize, page->m_CpuPtr, (*page)->GetGPUVirtualAddress()};
    }
    // use default size page
    // requeset new page
    if (m_CurrPage == nullptr || alignedSize > m_CurrPage->GetFreeSize())
        m_CurrPage = m_Pages.emplace_back(mgr.RequesetPage(m_Device));

    size_t                    offset = m_CurrPage->m_Offset;
    uint8_t*                  cpuPtr = m_CurrPage->m_CpuPtr + m_CurrPage->m_Offset;
    D3D12_GPU_VIRTUAL_ADDRESS gpuPtr = m_CurrPage->m_Resource->GetGPUVirtualAddress() + m_CurrPage->m_Offset;

    m_CurrPage->m_Offset += size;
    m_CurrPage->m_AllocationCount++;

    return {m_CurrPage, offset, size, cpuPtr, gpuPtr};
}

void LinearAllocator::SetFence(FenceValue fence) noexcept {
    // no allocation occur, just return.
    auto& mgr = sm_PageManager[static_cast<size_t>(m_Type)];
    for (auto&& page : m_Pages)
        mgr.DiscardPage(std::move(page), fence);
    m_Pages.clear();

    for (auto&& page : m_LargePages)
        mgr.DiscardLargePage(std::move(page), fence);
    m_LargePages.clear();
}

std::shared_ptr<LinearAllocator::LinearAllocationPage> LinearAllocator::PageManager::CreateNewPage(ID3D12Device* device, AllocationPageType type, size_t size) {
    CD3DX12_HEAP_PROPERTIES heapProp;
    D3D12_RESOURCE_STATES   defaultUsage;
    CD3DX12_RESOURCE_DESC   desc;

    switch (type) {
        case AllocationPageType::GPU_EXCLUSIVE: {
            heapProp     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            desc         = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            defaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        } break;
        case AllocationPageType::CPU_WRITABLE: {
            heapProp     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            desc         = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
            defaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
        } break;
        default: {
            throw std::bad_alloc();
        }
    }
    ID3D12Resource* resource = nullptr;
    ThrowIfFailed(device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, defaultUsage, nullptr, IID_PPV_ARGS(&resource)));
    resource->SetName(L"Linear Allocation Page");
    // resource ownership transfer to GpuResource
    auto ret = std::make_shared<LinearAllocationPage>(GpuResource{resource}, size);
    return ret;
}

void LinearAllocator::PageManager::UpdateAvailablePages(std::function<bool(FenceValue)>&& fenceChecker) {
    std::lock_guard lock(m_Mutex);

    for (auto iter = m_RetiredPages.begin(); iter != m_RetiredPages.end();) {
        auto&& [page, fence] = *iter;
        if (fenceChecker(fence) && page->m_AllocationCount == 0) {
            page->m_Offset = 0;
            m_AvailablePages.emplace(std::move(page));
            iter = m_RetiredPages.erase(iter);
        } else {
            iter++;
        }
    }

    for (auto iter = m_LargePages.begin(); iter != m_LargePages.end();) {
        auto&& [page, fence] = *iter;
        if (fenceChecker(fence)) {
            iter = m_LargePages.erase(iter);
        } else {
            iter++;
        }
    }
}

std::shared_ptr<LinearAllocator::LinearAllocationPage> LinearAllocator::PageManager::RequesetPage(ID3D12Device* device) {
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
}  // namespace Hitagi::Graphics::backend::DX12