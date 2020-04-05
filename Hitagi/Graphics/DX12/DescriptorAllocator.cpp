#include "D3D12GraphicsManager.hpp"
#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::DX12 {

// DescriptorAllocation
DescriptorAllocation::DescriptorAllocation()
    : m_CpuHandle{0}, m_numDescriptors(0), m_DescriptorSize(0), m_PageFrom(nullptr) {}

DescriptorAllocation::DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE handle, uint32_t numDescriptors,
                                           uint32_t descriptorSize, std::shared_ptr<DescriptorAllocatorPage> pageFrom)
    : m_CpuHandle(handle), m_numDescriptors(numDescriptors), m_DescriptorSize(descriptorSize), m_PageFrom(pageFrom) {}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& allocation)
    : m_CpuHandle(allocation.m_CpuHandle),
      m_numDescriptors(allocation.m_numDescriptors),
      m_DescriptorSize(allocation.m_DescriptorSize),
      m_PageFrom(std::move(allocation.m_PageFrom)) {
    allocation.m_CpuHandle.ptr  = 0;
    allocation.m_numDescriptors = 0;
    allocation.m_DescriptorSize = 0;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& rhs) {
    if (this != &rhs) {
        // TODO get fencevalue
        Free(0);
        m_CpuHandle      = rhs.m_CpuHandle;
        m_numDescriptors = rhs.m_numDescriptors;
        m_DescriptorSize = rhs.m_DescriptorSize;
        m_PageFrom       = std::move(rhs.m_PageFrom);

        rhs.m_CpuHandle.ptr  = 0;
        rhs.m_numDescriptors = 0;
        rhs.m_DescriptorSize = 0;
    }
    return *this;
}

DescriptorAllocation::~DescriptorAllocation() {
    // TODO RAII fenceValue
    Free(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorHandle(uint32_t offset) const {
    assert(offset < m_numDescriptors);
    return {m_CpuHandle.ptr + offset * m_DescriptorSize};
}

void DescriptorAllocation::Free(uint64_t fenceValue) {
    if (m_CpuHandle.ptr != 0 && m_PageFrom) {
        m_PageFrom->Free(std::move(*this), fenceValue);
        m_CpuHandle.ptr  = 0;
        m_numDescriptors = 0;
        m_DescriptorSize = 0;
        m_PageFrom.reset();
    }
}

// DescriptorAllocatorPage

DescriptorAllocatorPage::DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
    : m_Type(type), m_NumDescriptors(numDescriptors) {
    auto                       device = static_cast<D3D12GraphicsManager*>(g_GraphicsManager)->m_Device;
    D3D12_DESCRIPTOR_HEAP_DESC desc   = {};
    desc.Type                         = m_Type;
    desc.NumDescriptors               = m_NumDescriptors;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));

    m_BaseHandle          = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_HandleIncrementSize = device->GetDescriptorHandleIncrementSize(m_Type);
    m_NumFreeHandle       = m_NumDescriptors;

    AddNewBlock(0, m_NumFreeHandle);
}

DescriptorAllocation DescriptorAllocatorPage::Allocate(uint32_t numDescriptors) {
    std::lock_guard lock(m_AllocationMutex);

    if (numDescriptors > m_NumDescriptors) return DescriptorAllocation();

    auto sizeIt = m_FreeListBySize.lower_bound(numDescriptors);
    if (sizeIt == m_FreeListBySize.end()) return DescriptorAllocation();
    auto OffsetIt = GetOffsetIt(sizeIt);

    assert(sizeIt->second == OffsetIt->second && "BiMap Mapping error");

    auto blockSize   = sizeIt->first;
    auto blockOffset = OffsetIt->first;

    // Remove the block from free list;
    m_FreeIndexForIter.push(sizeIt->second);
    m_FreeListBySize.erase(sizeIt);
    m_FreeListByOffset.erase(OffsetIt);

    auto newOffset = blockOffset + numDescriptors;
    auto newSize   = blockSize - numDescriptors;
    if (newSize > 0) AddNewBlock(newOffset, newSize);

    m_NumFreeHandle -= numDescriptors;
    return DescriptorAllocation{CD3DX12_CPU_DESCRIPTOR_HANDLE(m_BaseHandle, blockOffset, m_HandleIncrementSize),
                                numDescriptors, m_HandleIncrementSize, shared_from_this()};
}

void DescriptorAllocatorPage::Free(DescriptorAllocation&& descriptor, uint64_t fenceValue) {
    auto            offset = ComputeOffset(descriptor.GetDescriptorHandle());
    std::lock_guard lock(m_AllocationMutex);
    // Don't add block to free list directly until the frame has finished executing on the GPU.
    m_StaleDescriptors.emplace(StaleDescriptorInfo{offset, descriptor.GetNumDescriptors(), fenceValue});
}

void DescriptorAllocatorPage::ReleaseStaleDescriptor(uint64_t fenceValue) {
    std::lock_guard lock(m_AllocationMutex);
    while (m_StaleDescriptors.empty() && m_StaleDescriptors.front().fenceValue <= fenceValue) {
        FreeBlock(m_StaleDescriptors.front().offset, m_StaleDescriptors.front().number);
        m_StaleDescriptors.pop();
    }
}

uint32_t DescriptorAllocatorPage::ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    return static_cast<uint32_t>((handle.ptr - m_BaseHandle.ptr) / m_HandleIncrementSize);
}

void DescriptorAllocatorPage::AddNewBlock(uint32_t offset, uint32_t numDescriptors) {
    if (m_FreeIndexForIter.empty()) {
        m_FreeIndexForIter.push(m_FreeListIter.size());
        m_FreeListIter.push_back({});
    }
    size_t index = m_FreeIndexForIter.front();

    m_FreeListIter[index].first  = m_FreeListByOffset.emplace(offset, index).first;
    m_FreeListIter[index].second = m_FreeListBySize.emplace(numDescriptors, index);
}

void DescriptorAllocatorPage::FreeBlock(uint32_t offset, uint32_t numDescriptors) {
    // |prevOffset        |offset        |nextOffset        |
    // |<----prevSize---->|<----size---->|<----nextSize---->|

    auto nextOffsetIt = m_FreeListByOffset.upper_bound(offset);
    auto prevOffsetIt = nextOffsetIt;
    if (prevOffsetIt != m_FreeListByOffset.begin())
        prevOffsetIt--;
    else
        prevOffsetIt = m_FreeListByOffset.end();

    // Upadte the number of free descriptors.
    m_NumFreeHandle += numDescriptors;

    // merge both freed block and the block to be freed.
    if (prevOffsetIt != m_FreeListByOffset.end() && prevOffsetIt->first + GetSizeIt(prevOffsetIt)->first == offset) {
        offset = prevOffsetIt->first;
        numDescriptors += GetSizeIt(prevOffsetIt)->first;

        m_FreeIndexForIter.push(prevOffsetIt->second);
        m_FreeListByOffset.erase(prevOffsetIt);
        m_FreeListBySize.erase(GetSizeIt(prevOffsetIt));
    }
    // |offset                           |nextOffset        |
    // |<------------size--------------->|<----nextSize---->|

    // merge both next block and the block to be freed.
    if (nextOffsetIt != m_FreeListByOffset.end() && offset + numDescriptors == nextOffsetIt->first) {
        numDescriptors += GetSizeIt(nextOffsetIt)->first;

        m_FreeIndexForIter.push(nextOffsetIt->second);
        m_FreeListByOffset.erase(nextOffsetIt);
        m_FreeListBySize.erase(GetSizeIt(nextOffsetIt));
    }
    // |offset                                              |
    // |<---------------------size------------------------->|

    // add the merged block to free list
    AddNewBlock(offset, numDescriptors);
}

// DescriptorAllocator

void DescriptorAllocator::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescPerHeap) {
    m_HeapType              = type;
    m_NumDescriptorsPerHeap = numDescPerHeap;
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::CreateAllocatorPage() {
    auto newPage = std::make_shared<DescriptorAllocatorPage>(m_HeapType, m_NumDescriptorsPerHeap);
    m_HeapPool.emplace_back(newPage);
    m_AvailbaleHeaps.insert(m_HeapPool.size() - 1);
    return newPage;
}

DescriptorAllocation DescriptorAllocator::Allocate(uint32_t numDescriptor) {
    std::lock_guard      lock(m_AllocationMutex);
    DescriptorAllocation allocation;

    for (auto iter = m_AvailbaleHeaps.begin(); iter != m_AvailbaleHeaps.end(); iter++) {
        auto allocatorPage = m_HeapPool[*iter];
        allocation         = allocatorPage->Allocate(numDescriptor);
        if (allocatorPage->NumFreeHandles() == 0) {
            iter = m_AvailbaleHeaps.erase(iter);
        }
        if (allocation) break;
    }
    if (!allocation) {
        m_NumDescriptorsPerHeap = std::max(m_NumDescriptorsPerHeap, numDescriptor);
        auto newPage            = CreateAllocatorPage();
        allocation              = newPage->Allocate(numDescriptor);
    };
    return allocation;
}

void DescriptorAllocator::ReleaseStaleDescriptor(uint64_t fenceValue) {
    std::lock_guard lock(m_AllocationMutex);

    for (size_t i = 0; i < m_HeapPool.size(); i++) {
        auto page = m_HeapPool[i];
        page->ReleaseStaleDescriptor(fenceValue);
        if (page->NumFreeHandles() > 0) {
            m_AvailbaleHeaps.insert(i);
        }
    }
}

}  // namespace Hitagi::Graphics::DX12