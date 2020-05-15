#include "DescriptorAllocator.hpp"
#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {

// DescriptorAllocation
DescriptorAllocation::DescriptorAllocation()
    : m_CpuHandle{0}, m_GpuHandle{0}, m_NumDescriptors(0), m_DescriptorSize(0), m_PageFrom(nullptr) {}

DescriptorAllocation::DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle, uint32_t numDescriptors,
                                           uint32_t descriptorSize, std::shared_ptr<DescriptorAllocatorPage> pageFrom)
    : m_CpuHandle(cpuHandle),
      m_GpuHandle(gpuHandle),
      m_NumDescriptors(numDescriptors),
      m_DescriptorSize(descriptorSize),
      m_PageFrom(pageFrom),
      m_FenceValue(0) {}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& allocation)
    : m_CpuHandle(allocation.m_CpuHandle),
      m_GpuHandle(allocation.m_GpuHandle),
      m_NumDescriptors(allocation.m_NumDescriptors),
      m_DescriptorSize(allocation.m_DescriptorSize),
      m_PageFrom(std::move(allocation.m_PageFrom)) {
    allocation.m_CpuHandle.ptr  = 0;
    allocation.m_GpuHandle.ptr  = 0;
    allocation.m_NumDescriptors = 0;
    allocation.m_DescriptorSize = 0;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& rhs) {
    if (this != &rhs) {
        if (m_PageFrom) m_PageFrom->Free(std::move(*this), m_FenceValue);
        m_CpuHandle      = rhs.m_CpuHandle;
        m_GpuHandle      = rhs.m_GpuHandle;
        m_NumDescriptors = rhs.m_NumDescriptors;
        m_DescriptorSize = rhs.m_DescriptorSize;
        m_PageFrom       = std::move(rhs.m_PageFrom);

        rhs.m_CpuHandle.ptr  = 0;
        rhs.m_GpuHandle.ptr  = 0;
        rhs.m_NumDescriptors = 0;
        rhs.m_DescriptorSize = 0;
    }
    return *this;
}

DescriptorAllocation::~DescriptorAllocation() {
    if (m_CpuHandle.ptr != 0 && m_PageFrom) {
        m_PageFrom->Free(std::move(*this), m_FenceValue);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorCpuHandle(uint32_t offset) const {
    assert(offset < m_NumDescriptors && m_CpuHandle.ptr != 0);
    return {m_CpuHandle.ptr + offset * m_DescriptorSize};
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorGpuHandle(uint32_t offset) const {
    assert(offset < m_NumDescriptors && m_GpuHandle.ptr != 0);
    return {m_GpuHandle.ptr + offset * m_DescriptorSize};
}

// DescriptorAllocatorPage

DescriptorAllocatorPage::DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flag)
    : m_Type(type), m_NumDescriptors(numDescriptors), m_Flag(flag) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type                       = m_Type;
    desc.NumDescriptors             = m_NumDescriptors;
    desc.Flags                      = flag;

    ThrowIfFailed(g_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));
    m_DescriptorHeap->SetName(L"DescriptorAllocatorPage");

    m_CpuBaseHandle       = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_GpuBaseHandle       = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    m_HandleIncrementSize = g_Device->GetDescriptorHandleIncrementSize(m_Type);
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
    auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CpuBaseHandle, blockOffset, m_HandleIncrementSize);
    auto gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GpuBaseHandle, blockOffset, m_HandleIncrementSize);
    return DescriptorAllocation{cpuHandle, gpuHandle, numDescriptors, m_HandleIncrementSize, shared_from_this()};
}

void DescriptorAllocatorPage::Free(DescriptorAllocation&& descriptor, uint64_t fenceValue) {
    auto            offset = ComputeOffset(descriptor.GetDescriptorCpuHandle());
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
    return static_cast<uint32_t>((handle.ptr - m_CpuBaseHandle.ptr) / m_HandleIncrementSize);
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

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescPerHeap) {
    m_HeapType              = type;
    m_NumDescriptorsPerHeap = numDescPerHeap;
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::CreateAllocatorPage() {
    auto newPage = std::make_shared<DescriptorAllocatorPage>(m_HeapType, m_NumDescriptorsPerHeap, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
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
        // Becase any page in availbal heap must not be empty,
        // so when NumFreeHandless return 0, allocating must have happend
        // and we can erase it safely.
        if (allocatorPage->NumFreeHandles() == 0) {
            iter = m_AvailbaleHeaps.erase(iter);
            assert(allocation);
        }
        if (allocation) break;
    }
    if (!allocation) {
        m_NumDescriptorsPerHeap = std::max(m_NumDescriptorsPerHeap, numDescriptor);
        auto newPage            = CreateAllocatorPage();
        allocation              = newPage->Allocate(numDescriptor);

        if (newPage->NumFreeHandles() == 0) m_AvailbaleHeaps.erase(m_HeapPool.size() - 1);
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

void UserDescriptorHeap::Initalize(const std::wstring_view name, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                   uint32_t count) {
    D3D12_DESCRIPTOR_HEAP_FLAGS flag;
    if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        flag = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    else
        flag = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    m_Page = std::make_shared<DescriptorAllocatorPage>(type, count, flag);
    m_Page->GetHeapPointer()->SetName(name.data());
}

DescriptorAllocation UserDescriptorHeap::Allocate(uint32_t numDescriptor) {
    assert(m_Page && "Does the User Descriptor be Initialized?");
    return m_Page->Allocate(numDescriptor);
}

}  // namespace Hitagi::Graphics::DX12