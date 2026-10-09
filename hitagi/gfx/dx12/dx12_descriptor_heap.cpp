module;
#include "interop/win32_macros.hpp"
#include <cassert>

export module gfx.dx12:descriptor_heap;
#ifdef _WIN32
import interop.win32;
#endif
import interop.spdlog;
import interop.dx12;
import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :utils;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

class Descriptor {
public:
    Descriptor()                             = default;
    Descriptor(const Descriptor&)            = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) noexcept;
    Descriptor& operator=(Descriptor&&) noexcept;
    ~Descriptor();

    inline operator bool() const noexcept { return m_HeapFrom != nullptr && m_CPUHandle.ptr != 0; }

    inline const auto& GetCPUHandle() const noexcept { return m_CPUHandle; }

private:
    friend class DescriptorHeap;
    Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, DescriptorHeap* heap_from);

    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {0};
    DescriptorHeap*             m_HeapFrom  = nullptr;
};

class DescriptorHeap {
public:
    DescriptorHeap(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptors, std::string_view name = "");

    bool               Empty() const;
    [[nodiscard]] auto Allocate() -> Descriptor;
    void               DiscardDescriptor(Descriptor& descriptor);

private:
    mutable std::mutex m_Mutex;

    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_HeapCPUStart;
    std::size_t                  m_IncrementSize;
    D3D12_DESCRIPTOR_HEAP_TYPE   m_Type;

    std::pmr::deque<Descriptor> m_AvailableDescriptors;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptor_per_page = 1024);

    [[nodiscard]] auto Allocate() -> Descriptor;

private:
    ID3D12Device&                                    m_Device;
    std::shared_ptr<spdlog::logger>                  m_Logger;
    D3D12_DESCRIPTOR_HEAP_TYPE                       m_Type;
    std::size_t                                      m_HeapSize;
    std::pmr::deque<std::shared_ptr<DescriptorHeap>> m_HeapPool;
};

}  // namespace hitagi::gfx

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

DescriptorHeap::DescriptorHeap(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptors, std::string_view name) : m_Type(type) {
    m_IncrementSize = device.GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type           = type,
        .NumDescriptors = static_cast<UINT>(num_descriptors),
        .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask       = 0,
    };

    if (interop::failed(device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)))) {
        logger->error("failed to create descriptor heap");
        throw std::runtime_error("failed to create descriptor heap");
    }
    if (!name.empty()) {
        m_DescriptorHeap->SetName(std::wstring(name.begin(), name.end()).c_str());
    }
    m_HeapCPUStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle{m_HeapCPUStart};
    for (std::size_t i = 0; i < num_descriptors; ++i) {
        // We will enable descriptor handle in Allocate()
        m_AvailableDescriptors.emplace_back(Descriptor(handle, nullptr));
        handle.Offset(static_cast<INT>(m_IncrementSize));
    }
}

bool DescriptorHeap::Empty() const {
    std::scoped_lock lock{m_Mutex};
    return m_AvailableDescriptors.empty();
}

auto DescriptorHeap::Allocate() -> Descriptor {
    std::scoped_lock lock{m_Mutex};

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
    std::scoped_lock lock{m_Mutex};
    // Disable descriptor
    m_AvailableDescriptors.emplace_back(Descriptor(descriptor.GetCPUHandle(), nullptr));
}

DescriptorAllocator::DescriptorAllocator(ID3D12Device& device, std::shared_ptr<spdlog::logger> logger, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptor_per_heap)
    : m_Device(device), m_Logger(std::move(logger)), m_Type(type), m_HeapSize(num_descriptor_per_heap) {
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
            name = std::format("RTV_Heap_{}", m_HeapPool.size());
        } else {
            name = std::format("DSV_Heap_{}", m_HeapPool.size());
        }
        heap_for_allocating = m_HeapPool.emplace_front(std::make_shared<DescriptorHeap>(m_Device, m_Logger, m_Type, m_HeapSize, name));
    }

    return heap_for_allocating->Allocate();
}

}  // namespace hitagi::gfx
