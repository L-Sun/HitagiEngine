#pragma once
#include "d3d_pch.hpp"

#include <optional>

// the DescriptorAllocator is CPU visiable only, for those used shader visiable descriptor,
// use the dynamic descriptor heap (in context).
namespace hitagi::graphics::backend::DX12 {

class DescriptorPage;

struct Descriptor {
    Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::shared_ptr<DescriptorPage> page_from);
    Descriptor(const Descriptor& rhs) : handle(rhs.handle), page_from(nullptr) {}
    Descriptor& operator=(const Descriptor& rhs) {
        if (this != &rhs) handle = rhs.handle;
        return *this;
    }
    Descriptor(Descriptor&&) = default;
    Descriptor& operator=(Descriptor&&) = default;

    ~Descriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE     handle;
    std::shared_ptr<DescriptorPage> page_from;
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

    std::optional<std::vector<Descriptor>> Allocate(size_t num_descriptors);

protected:
    DescriptorPage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptors);

private:
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_Handle{};
    size_t                       m_IncrementSize;

    using BlockSize   = size_t;
    using BlockOffset = size_t;
    using SizeIter    = std::multimap<size_t, size_t>::iterator;

    // * according to cpp17 std www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf [26.2.6/9] [26.2.6/10]
    // * For associative containers `insert` and `emplace` members shall not affect the validity of iterators to the container
    // * and `erase` and `extract` members invalidate only iterators to the erased elements that we will not use in the future
    std::map<BlockOffset, SizeIter>       m_AvailableDescriptors;
    std::multimap<BlockSize, BlockOffset> m_SearchMap;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptor_per_page = 1024);
    void Initialize(ID3D12Device* device);

    Descriptor                 Allocate();
    std::vector<Descriptor>    Allocate(size_t num_descriptors);
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_Type; }

private:
    ID3D12Device*                              m_Device{};
    D3D12_DESCRIPTOR_HEAP_TYPE                 m_Type;
    size_t                                     m_NumDescriptorPerPage;
    std::list<std::shared_ptr<DescriptorPage>> m_PagePool;
};

}  // namespace hitagi::graphics::backend::DX12