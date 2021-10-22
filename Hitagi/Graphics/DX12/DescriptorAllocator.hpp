#pragma once
#include "D3Dpch.hpp"
#include <optional>

// the DescriptorAllocator is CPU visiable only, for those used shader visiable descriptor,
// use the dynamic descriptor heap (in context).
namespace Hitagi::Graphics::backend::DX12 {

class DescriptorPage;

struct Descriptor {
    Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::shared_ptr<DescriptorPage> pageFrom);
    Descriptor(const Descriptor& rhs) : handle(rhs.handle), pageFrom(nullptr) {}
    Descriptor& operator=(const Descriptor& rhs) {
        if (this != &rhs) handle = rhs.handle;
        return *this;
    }
    Descriptor(Descriptor&&) = default;
    Descriptor& operator=(Descriptor&&) = default;

    ~Descriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE     handle;
    std::shared_ptr<DescriptorPage> pageFrom;
};

class DescriptorPage : public std::enable_shared_from_this<DescriptorPage> {
public:
    static std::shared_ptr<DescriptorPage>
    Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors) {
        struct createPage : public DescriptorPage {
            createPage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors)
                : DescriptorPage(device, type, numDescriptors) {}
        };
        return std::dynamic_pointer_cast<DescriptorPage>(std::make_shared<createPage>(device, type, numDescriptors));
    }

    void DiscardDescriptor(Descriptor& descriptor);

    std::optional<std::vector<Descriptor>> Allocate(size_t numDescriptors);

protected:
    DescriptorPage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors);

private:
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_Handle;
    size_t                       m_IncrementSize;

    using BlockSize   = size_t;
    using BlockOffset = size_t;
    // TODO iter will change if the map is reallocated.
    using SizeIter = std::multimap<size_t, size_t>::iterator;

    std::map<BlockOffset, SizeIter>       m_AvailableDescriptors;
    std::multimap<BlockSize, BlockOffset> m_SearchMap;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptorPerPage = 1024);
    void Initialize(ID3D12Device* device);

    Descriptor                 Allocate();
    std::vector<Descriptor>    Allocate(size_t numDescriptors);
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_Type; }

private:
    ID3D12Device*                              m_Device;
    D3D12_DESCRIPTOR_HEAP_TYPE                 m_Type;
    size_t                                     m_NumDescriptorPerPage;
    std::list<std::shared_ptr<DescriptorPage>> m_PagePool;
};

}  // namespace Hitagi::Graphics::backend::DX12