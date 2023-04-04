#include "dx12_descriptor_heap.hpp"
#include "dx12_device.hpp"

#include <d3dx12/d3dx12.h>
#include <spdlog/logger.h>

namespace hitagi::gfx {

Descriptor::Descriptor(Descriptor&& other) noexcept
    : offset_in_heap(other.offset_in_heap),
      cpu_handle(other.cpu_handle),
      gpu_handle(other.gpu_handle),
      num(other.num),
      increment_size(other.increment_size),
      heap_from(other.heap_from) {
    other.cpu_handle.ptr = 0;
    other.gpu_handle.ptr = 0;
    other.num            = 0;
    other.increment_size = 0;
    other.heap_from      = nullptr;
}

Descriptor& Descriptor::operator=(Descriptor&& rhs) noexcept {
    if (this != &rhs) {
        if (heap_from) heap_from->DiscardDescriptor(*this);
        offset_in_heap = rhs.offset_in_heap;
        cpu_handle     = rhs.cpu_handle;
        gpu_handle     = rhs.gpu_handle;
        num            = rhs.num;
        increment_size = rhs.increment_size,
        heap_from      = rhs.heap_from;

        rhs.offset_in_heap = std::numeric_limits<std::size_t>::max();
        rhs.cpu_handle.ptr = 0;
        rhs.gpu_handle.ptr = 0;
        rhs.num            = 0;
        rhs.increment_size = 0,
        rhs.heap_from      = nullptr;
    }
    return *this;
}

Descriptor::~Descriptor() {
    if (heap_from) heap_from->DiscardDescriptor(*this);
}

DescriptorHeap::DescriptorHeap(DX12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shader_visible, std::size_t num_descriptors, std::string_view name) : m_Type(type) {
    m_IncrementSize = device.GetDevice()->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type           = type,
        .NumDescriptors = static_cast<UINT>(num_descriptors),
        .Flags          = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
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
    if (shader_visible)
        m_HeapGPUStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    else
        m_HeapGPUStart.ptr = 0;

    auto iter = m_SearchMap.emplace(num_descriptors, 0);
    m_AvailableDescriptors.emplace(0, iter);
}

auto DescriptorHeap::AvailableSize() const -> std::size_t {
    std::lock_guard lock{m_Mutex};
    if (m_SearchMap.empty()) return 0;
    return m_SearchMap.rbegin()->first;
}

auto DescriptorHeap::Allocate(std::size_t num_descriptors) -> Descriptor {
    std::lock_guard lock{m_Mutex};

    assert(num_descriptors != 0);
    auto iter = m_SearchMap.lower_bound(num_descriptors);
    if (iter == m_SearchMap.end()) return {};

    // update block
    auto        block_info = m_SearchMap.extract(iter);
    BlockOffset offset     = block_info.mapped();
    auto        block      = m_AvailableDescriptors.extract(offset);

    // Now, we have the block info, offset and size, and then generate descriptors from the back of block
    Descriptor result;
    result.cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE{
        m_HeapCPUStart,
        static_cast<INT>(offset),
        static_cast<UINT>(m_IncrementSize),
    };
    if (m_HeapGPUStart.ptr != 0) {
        result.gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            m_HeapGPUStart,
            static_cast<INT>(offset),
            static_cast<UINT>(m_IncrementSize),
        };
    }
    result.offset_in_heap = offset;
    result.num            = num_descriptors;
    result.increment_size = m_IncrementSize;
    result.heap_from      = this;

    block_info.key() -= num_descriptors;
    if (block_info.key() != 0) {
        block_info.mapped() = offset + num_descriptors;
        block.key()         = block_info.mapped();
        block.mapped()      = m_SearchMap.insert(std::move(block_info));
        m_AvailableDescriptors.insert(std::move(block));
    }

    return result;
}

void DescriptorHeap::DiscardDescriptor(Descriptor& descriptor) {
    std::lock_guard lock{m_Mutex};

    auto        offset = (descriptor.cpu_handle.ptr - m_HeapCPUStart.ptr) / m_IncrementSize;
    std::size_t size   = descriptor.num;
    if (m_AvailableDescriptors.empty()) {
        auto iter = m_SearchMap.emplace(1, offset);
        m_AvailableDescriptors.emplace(offset, iter);
        return;
    }

    auto right = m_AvailableDescriptors.upper_bound(offset);
    auto left  = right != m_AvailableDescriptors.begin()
                     ? std::prev(right)
                     : m_AvailableDescriptors.end();

    // Merge forward
    if (left != m_AvailableDescriptors.end()) {
        BlockOffset block_offset = left->first;
        BlockSize   block_size   = left->second->first;
        if (block_offset + block_size == offset) {
            offset = block_offset;
            size += block_size;
            m_SearchMap.erase(left->second);
            m_AvailableDescriptors.erase(left);
        }
    }

    // Merge backward
    if (right != m_AvailableDescriptors.end()) {
        BlockOffset block_offset = right->first;
        BlockSize   block_size   = right->second->first;
        if (offset + size == block_offset) {
            size += block_size;
            m_SearchMap.erase(right->second);
            m_AvailableDescriptors.erase(right);
        }
    }

    // insert the merged block
    m_AvailableDescriptors.emplace(offset, m_SearchMap.emplace(size, offset));
}

DescriptorAllocator::DescriptorAllocator(DX12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shader_visible, std::size_t num_descriptor_per_heap)
    : m_Device(device), m_Type(type), m_Visibility(shader_visible), m_HeapSize(num_descriptor_per_heap) {
}

auto DescriptorAllocator::Allocate(std::size_t num_descriptors) -> Descriptor {
    if (num_descriptors == 0) {
        m_Device.GetLogger()->error("Can not allocate zero descriptor!");
        throw std::invalid_argument("Can not allocate zero descriptor!");
    }

    // the Size larger than current heap size, so update default heap size,
    // and create large heap
    m_HeapSize = std::max(m_HeapSize, num_descriptors);

    std::shared_ptr<DescriptorHeap> heap_for_allocating = nullptr;
    // Search heap that have enough size to allocate descriptors.
    for (const auto& heap : m_HeapPool) {
        if (heap->AvailableSize() >= num_descriptors) {
            heap_for_allocating = heap;
            break;
        }
    }

    if (heap_for_allocating == nullptr) {
        // Need create new heap
        heap_for_allocating = m_HeapPool.emplace_front(std::make_shared<DescriptorHeap>(m_Device, m_Type, m_Visibility, m_HeapSize));
    }

    return heap_for_allocating->Allocate(num_descriptors);
}

}  // namespace hitagi::gfx