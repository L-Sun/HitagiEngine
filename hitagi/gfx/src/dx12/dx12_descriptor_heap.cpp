#include "dx12_descriptor_heap.hpp"
#include "dx12_device.hpp"

#include <d3dx12/d3dx12.h>
#include <spdlog/logger.h>

namespace hitagi::gfx {

Descriptor::Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, DescriptorHeap* heap_from)
    : m_CPUHandle(cpu_handle), m_HeapFrom(heap_from) {
    assert(cpu_handle.ptr != 0);
}

Descriptor::Descriptor(Descriptor&& other) noexcept
    : m_CPUHandle(other.m_CPUHandle),
      m_HeapFrom(other.m_HeapFrom) {
    other.m_CPUHandle.ptr = 0;
    other.m_HeapFrom      = nullptr;
}

Descriptor& Descriptor::operator=(Descriptor&& rhs) noexcept {
    if (this != &rhs) {
        if (m_HeapFrom) m_HeapFrom->DiscardDescriptor(*this);
        m_CPUHandle = rhs.m_CPUHandle;
        m_HeapFrom  = rhs.m_HeapFrom;

        rhs.m_CPUHandle.ptr = 0;
        rhs.m_HeapFrom      = nullptr;
    }
    return *this;
}

Descriptor::~Descriptor() {
    if (m_HeapFrom) m_HeapFrom->DiscardDescriptor(*this);
}

DescriptorHeap::DescriptorHeap(DX12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptors, std::string_view name) : m_Type(type) {
    m_IncrementSize = device.GetDevice()->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type           = type,
        .NumDescriptors = static_cast<UINT>(num_descriptors),
        .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask       = 0,
    };

    if (FAILED(device.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)))) {
        device.GetLogger()->error("failed to create descriptor heap");
        throw std::runtime_error("failed to create descriptor heap");
    }
    if (!name.empty()) {
        m_DescriptorHeap->SetName(std::wstring(name.begin(), name.end()).c_str());
    }
    m_HeapCPUStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle{m_HeapCPUStart};
    for (std::size_t i = 0; i < num_descriptors; ++i) {
        // We will enable descriptor handle in Allocate()
        m_AvailableDescriptors.emplace_back(Descriptor(handle.Offset(static_cast<INT>(m_IncrementSize)), nullptr));
    }
}

bool DescriptorHeap::Empty() const {
    std::lock_guard lock{m_Mutex};
    return m_AvailableDescriptors.empty();
}

auto DescriptorHeap::Allocate() -> Descriptor {
    std::lock_guard lock{m_Mutex};

    if (m_AvailableDescriptors.empty()) {
        throw std::runtime_error("no descriptor available");
    }

    auto result = std::move(m_AvailableDescriptors.front());
    m_AvailableDescriptors.pop_front();

    // Enable descriptor
    result.m_HeapFrom = this;
    return result;
}

void DescriptorHeap::DiscardDescriptor(Descriptor& descriptor) {
    std::lock_guard lock{m_Mutex};
    // Disable descriptor
    m_AvailableDescriptors.emplace_back(Descriptor(descriptor.GetCPUHandle(), nullptr));
}

DescriptorAllocator::DescriptorAllocator(DX12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptor_per_heap)
    : m_Device(device), m_Type(type), m_HeapSize(num_descriptor_per_heap) {
    assert(m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

auto DescriptorAllocator::Allocate() -> Descriptor {
    std::shared_ptr<DescriptorHeap> heap_for_allocating = nullptr;
    // Search heap that have enough size to allocate descriptors.
    for (const auto& heap : m_HeapPool) {
        if (!heap->Empty()) {
            heap_for_allocating = heap;
            break;
        }
    }

    if (heap_for_allocating == nullptr) {
        // Need create new heap
        std::string name;
        if (m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            name = fmt::format("RTV_Heap_{}", m_HeapPool.size());
        } else {
            name = fmt::format("DSV_Heap_{}", m_HeapPool.size());
        }
        heap_for_allocating = m_HeapPool.emplace_front(std::make_shared<DescriptorHeap>(m_Device, m_Type, m_HeapSize, name));
    }

    return heap_for_allocating->Allocate();
}

}  // namespace hitagi::gfx