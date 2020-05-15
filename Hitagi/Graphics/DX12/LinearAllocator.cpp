#include "LinearAllocator.hpp"

#include <new>

#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {

std::array<LinearAllocatorPageManager, static_cast<unsigned>(LinearAllocatorType::NUM_TYPES)>
    LinearAllocator::sm_PageManager = {
        LinearAllocatorPageManager(LinearAllocatorType::GPU_EXCLUSIVE),
        LinearAllocatorPageManager(LinearAllocatorType::CPU_WRITABLE),
};

LinearAllocatorPageManager::LinearAllocatorPageManager(LinearAllocatorType type) : m_Type(type) {}

LinearAllocationPage* LinearAllocatorPageManager::RequestPage() {
    std::lock_guard lock(m_Mutex);
    while (!m_RetiredPages.empty() && g_CommandManager.IsFenceComplete(m_RetiredPages.front().first)) {
        m_AvailablePages.push(m_RetiredPages.front().second);
        m_RetiredPages.pop();
    }

    LinearAllocationPage* ret;

    if (!m_AvailablePages.empty()) {
        ret = m_AvailablePages.front();
        m_AvailablePages.pop();
    } else {
        auto newPage = CreateNewPage(m_Type == LinearAllocatorType::GPU_EXCLUSIVE ? GPU_ALLOCATOR_PAGE_SIZE
                                                                                  : CPU_ALLOCATOR_PAGE_SIZE);
        ret          = newPage.get();
        m_PagePool.emplace_back(newPage.release());
    }

    return ret;
}

std::pair<size_t, LinearAllocationPage*> LinearAllocatorPageManager::RequestLargePage(size_t pageSize) {
    std::lock_guard lock(m_Mutex);
    auto            newPage = CreateNewPage(pageSize);
    auto            ret     = newPage.get();
    size_t          index;
    if (!m_LargePagesEmptyIndex.empty()) {
        index = m_LargePagesEmptyIndex.front();
        m_LargePagesEmptyIndex.pop();
        m_LargePages[index].reset(newPage.release());
    } else {
        index = m_LargePages.size();
        m_LargePages.emplace_back(newPage.release());
    }
    return {index, ret};
}

std::unique_ptr<LinearAllocationPage> LinearAllocatorPageManager::CreateNewPage(size_t pageSize) {
    CD3DX12_HEAP_PROPERTIES heapProp;
    D3D12_RESOURCE_STATES   defaultUsage;
    CD3DX12_RESOURCE_DESC   desc;

    switch (m_Type) {
        case LinearAllocatorType::GPU_EXCLUSIVE: {
            heapProp     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            desc         = CD3DX12_RESOURCE_DESC::Buffer(pageSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            defaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        } break;
        case LinearAllocatorType::CPU_WRITABLE: {
            heapProp     = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            desc         = CD3DX12_RESOURCE_DESC::Buffer(pageSize, D3D12_RESOURCE_FLAG_NONE);
            defaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
        } break;
        default: {
            throw std::bad_alloc();
        }
    }
    ID3D12Resource* buffer;
    ThrowIfFailed(g_Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, defaultUsage, nullptr,
                                                    IID_PPV_ARGS(&buffer)));
    buffer->SetName(L"LinearAllocator Page");
    return std::make_unique<LinearAllocationPage>(buffer, defaultUsage);
}

void LinearAllocatorPageManager::DiscardPages(uint64_t fenceValue, const std::vector<LinearAllocationPage*>& pages) {
    std::lock_guard lock(m_Mutex);
    for (auto&& page : pages) m_RetiredPages.push({fenceValue, page});
}

void LinearAllocatorPageManager::FreeLargePages(uint64_t fenceValue, const std::vector<size_t>& largePageIndex) {
    std::lock_guard lock(m_Mutex);
    while (!m_DeletionQueue.empty() && g_CommandManager.IsFenceComplete(m_DeletionQueue.front().first)) {
        m_LargePages[m_DeletionQueue.front().second].reset();
        m_LargePagesEmptyIndex.push(m_DeletionQueue.front().second);
        m_DeletionQueue.pop();
    }

    for (auto&& index : largePageIndex) {
        (*m_LargePages[index])->Unmap(0, nullptr);
        m_DeletionQueue.push({fenceValue, index});
    }
}

LinearAllocation LinearAllocator::Allocate(size_t size, size_t alignment) {
    size_t alignedSize = align(size, alignment);
    if (alignedSize > m_PageSize) return AllocateLargePage(alignedSize);
    if (m_CurrOffset + alignedSize > m_PageSize) {
        assert(m_CurrPage != nullptr);
        m_RetiredPages.push_back(m_CurrPage);
        m_CurrPage = nullptr;
    }

    if (m_CurrPage == nullptr) {
        m_CurrPage   = sm_PageManager[static_cast<int>(m_Type)].RequestPage();
        m_CurrOffset = 0;
    }

    LinearAllocation ret{*m_CurrPage, m_CurrOffset, alignedSize,
                         reinterpret_cast<uint8_t*>(m_CurrPage->m_CpuPtr) + m_CurrOffset,
                         m_CurrPage->m_GpuVirtualAddress + m_CurrOffset};
    m_CurrOffset += alignedSize;
    return ret;
}

LinearAllocation LinearAllocator::AllocateLargePage(size_t size) {
    auto [index, page] = sm_PageManager[static_cast<int>(m_Type)].RequestLargePage(size);
    m_LargePageIndex.push_back(index);

    return LinearAllocation{*page, 0, size, page->m_CpuPtr, page->m_GpuVirtualAddress};
}

void LinearAllocator::DiscardPages(uint64_t fenceValue) {
    if (m_CurrPage) {
        m_RetiredPages.push_back(m_CurrPage);
        m_CurrPage   = nullptr;
        m_CurrOffset = 0;
    }

    sm_PageManager[static_cast<int>(m_Type)].DiscardPages(fenceValue, m_RetiredPages);
    m_RetiredPages.clear();
    sm_PageManager[static_cast<int>(m_Type)].FreeLargePages(fenceValue, m_LargePageIndex);
    m_LargePageIndex.clear();
}

}  // namespace Hitagi::Graphics::DX12