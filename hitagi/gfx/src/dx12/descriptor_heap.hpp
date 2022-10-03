#pragma once
#include <d3d12.h>
#include <wrl.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include <list>
#include <memory>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;
class DescriptorHeap;

struct Descriptor {
    Descriptor()                             = default;
    Descriptor(const Descriptor&)            = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) noexcept;
    Descriptor& operator=(Descriptor&&) noexcept;
    ~Descriptor();

    inline operator bool() const noexcept { return heap_from != nullptr; }

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {0};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {0};
    // When it contains multiple descriptor, the following member variable may be useful
    std::size_t     num = 0;
    std::size_t     increament_size;
    DescriptorHeap* heap_from = nullptr;
};

class DescriptorHeap {
public:
    DescriptorHeap(DX12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shader_visiable, std::size_t num_descriptors);

    auto AvaliableSize() -> std::size_t;
    auto Allocate(std::size_t num_descriptors) -> Descriptor;
    void DiscardDescriptor(Descriptor& descriptor);

    inline ID3D12DescriptorHeap* GetHeap() const noexcept { return m_DescriptorHeap.Get(); }

private:
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_HeapCpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE  m_HeapGpuStart;
    std::size_t                  m_IncrementSize;
    D3D12_DESCRIPTOR_HEAP_TYPE   m_Type;

    using BlockSize   = std::size_t;
    using BlockOffset = std::size_t;
    using SearchMap   = std::pmr::multimap<BlockSize, BlockOffset>;

    // * according to cpp17 std www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf [26.2.6/9] [26.2.6/10]
    // * For associative containers `insert` and `emplace` members shall not affect the validity of iterators to the container
    // * and `erase` and `extract` members invalidate only iterators to the erased elements that we will not use in the future
    std::pmr::map<BlockOffset, SearchMap::iterator> m_AvailableDescriptors;
    SearchMap                                       m_SearchMap;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(DX12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shader_visiable, std::size_t num_descriptor_per_page = 1024);

    auto Allocate(std::size_t num = 1) -> Descriptor;

private:
    DX12Device*                                     m_Device;
    D3D12_DESCRIPTOR_HEAP_TYPE                      m_Type;
    bool                                            m_Visibility;
    std::size_t                                     m_HeapSize;
    std::pmr::list<std::shared_ptr<DescriptorHeap>> m_HeapPool;
};

}  // namespace hitagi::gfx