#pragma once
#include <d3d12.h>
#include <wrl.h>

#include <mutex>
#include <deque>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;
class DescriptorHeap;

// Only use for RTV and DSV on cpu side

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
    DescriptorHeap(DX12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptors, std::string_view name = "");

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
    DescriptorAllocator(DX12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t num_descriptor_per_page = 1024);

    [[nodiscard]] auto Allocate() -> Descriptor;

private:
    DX12Device&                                      m_Device;
    D3D12_DESCRIPTOR_HEAP_TYPE                       m_Type;
    std::size_t                                      m_HeapSize;
    std::pmr::deque<std::shared_ptr<DescriptorHeap>> m_HeapPool;
};

}  // namespace hitagi::gfx