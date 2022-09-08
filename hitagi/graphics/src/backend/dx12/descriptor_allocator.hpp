#pragma once
#include "d3d_pch.hpp"

// the DescriptorAllocator is CPU visiable only, for those used shader visiable descriptor,
// use the dynamic descriptor heap (in context).
namespace hitagi::gfx::backend::DX12 {

class DescriptorPage;

struct Descriptor {
    enum struct Type : std::uint8_t {
        CBV,
        SRV,
        UAV,
        Sampler,
        RTV,
        DSV,
    };

    Descriptor() = default;
    Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::shared_ptr<DescriptorPage> page_from, Type type);
    Descriptor(const Descriptor&)            = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&)                 = default;
    Descriptor& operator=(Descriptor&&)      = default;

    operator bool() const noexcept { return handle.ptr != 0 && page_from != nullptr; }

    ~Descriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE     handle    = {0};
    std::shared_ptr<DescriptorPage> page_from = nullptr;
    Type                            type;
};

class DescriptorPage : public std::enable_shared_from_this<DescriptorPage> {
public:
    static std::shared_ptr<DescriptorPage>
    Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptors) {
        struct CreatePage : public DescriptorPage {
            CreatePage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptors)
                : DescriptorPage(device, type, num_descriptors) {}
        };
        return std::dynamic_pointer_cast<DescriptorPage>(std::make_shared<CreatePage>(device, type, num_descriptors));
    }

    void DiscardDescriptor(Descriptor& descriptor);

    std::pmr::vector<Descriptor> Allocate(size_t num_descriptors, Descriptor::Type type);

protected:
    DescriptorPage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptors);

private:
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_Handle{};
    size_t                       m_IncrementSize;

    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;

    using BlockSize   = size_t;
    using BlockOffset = size_t;
    using SearchMap   = std::pmr::multimap<BlockSize, BlockOffset>;

    // * according to cpp17 std www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf [26.2.6/9] [26.2.6/10]
    // * For associative containers `insert` and `emplace` members shall not affect the validity of iterators to the container
    // * and `erase` and `extract` members invalidate only iterators to the erased elements that we will not use in the future
    std::pmr::map<BlockOffset, SearchMap::iterator> m_AvailableDescriptors;
    SearchMap                                       m_SearchMap;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptor_per_page = 1024);
    void Initialize(ID3D12Device* device);

    Descriptor                   Allocate(Descriptor::Type type);
    std::pmr::vector<Descriptor> Allocate(size_t num_descriptors, Descriptor::Type type);
    D3D12_DESCRIPTOR_HEAP_TYPE   GetHeapType() const { return m_HeapType; }

private:
    ID3D12Device*                                   m_Device{};
    D3D12_DESCRIPTOR_HEAP_TYPE                      m_HeapType;
    size_t                                          m_NumDescriptorPerPage;
    std::pmr::list<std::shared_ptr<DescriptorPage>> m_PagePool;
};

}  // namespace hitagi::gfx::backend::DX12