#include "LinearAllocator.hpp"

#include <new>

namespace Hitagi::Graphics::DX12 {
size_t align(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }

void LinearAllocator::Initalize(ID3D12Device6* device, size_t pageSize) {
    if (!m_Initialized) {
        m_Device      = device;
        m_PageSize    = pageSize;
        m_Initialized = true;
    }
}

LinearAllocator::Allocation LinearAllocator::Allocate(size_t size, size_t alignment) {
    if (!m_Initialized || size > m_PageSize) throw std::bad_alloc();
    if (!m_CurrPage || !m_CurrPage->HasSpace(size, alignment)) m_CurrPage = RequestPage();

    return m_CurrPage->Allocate(size, alignment);
}

std::shared_ptr<LinearAllocator::Page> LinearAllocator::RequestPage() {
    std::shared_ptr<Page> page;
    if (!m_AvailablePages.empty()) {
        page = m_AvailablePages.front();
        m_AvailablePages.pop();
    } else {
        page = std::make_shared<Page>(m_Device, m_PageSize);
        m_PagePool.push_back(page);
    }
    return page;
}

void LinearAllocator::Reset() {
    m_CurrPage = nullptr;
    while (!m_AvailablePages.empty()) m_AvailablePages.pop();
    for (auto&& page : m_PagePool) {
        page->Reset();
        m_AvailablePages.push(page);
    }
}

LinearAllocator::~LinearAllocator() {}

LinearAllocator::Page::Page(ID3D12Device6* device, size_t pageSize)
    : m_Device(device), m_PageSize(pageSize), m_Offset(0), m_CpuPtr(nullptr) {
    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto desc     = CD3DX12_RESOURCE_DESC::Buffer(m_PageSize);
    m_UsageState  = D3D12_RESOURCE_STATE_GENERIC_READ;
    ThrowIfFailed(m_Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, m_UsageState, nullptr,
                                                    IID_PPV_ARGS(&m_Resource)));
    UpdateGpuVirtualAddress();

    m_Resource->Map(0, nullptr, &m_CpuPtr);
}
LinearAllocator::Page::~Page() {
    m_Resource->Unmap(0, nullptr);
    m_CpuPtr = nullptr;
}

bool LinearAllocator::Page::HasSpace(size_t size, size_t alignment) const {
    assert(alignment > 0 && ((alignment & (alignment - 1))) == 0);
    size_t alignedSize   = align(size, alignment);
    size_t alignedOffset = align(m_Offset, alignment);
    return alignedOffset + alignedSize <= m_PageSize;
}

LinearAllocator::Allocation LinearAllocator::Page::Allocate(size_t size, size_t alignment) {
    if (!HasSpace(size, alignment)) throw std::bad_alloc();

    size_t alignedSize = align(size, alignment);
    m_Offset           = align(m_Offset, alignment);
    Allocation ret     = {*this, m_Offset, alignedSize, static_cast<uint8_t*>(m_CpuPtr) + m_Offset,
                      m_GpuVirtualAddress + m_Offset};
    m_Offset += alignedSize;
    return ret;
}

void LinearAllocator::Page::Reset() { m_Offset = 0; }

}  // namespace Hitagi::Graphics::DX12