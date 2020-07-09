#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::backend::DX12 {
Descriptor::Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::shared_ptr<DescriptorPage> pageFrom)
    : handle(std::move(handle)), pageFrom(std::move(pageFrom)) {}

Descriptor::~Descriptor() {
    if (pageFrom) pageFrom->DiscardDescriptor(*this);
}

DescriptorPage::DescriptorPage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors) {
    assert(device);
    m_IncrementSize = device->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc;
    desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // Just CPU Visiable
    desc.NodeMask       = 0;
    desc.NumDescriptors = numDescriptors;
    desc.Type           = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));
    m_DescriptorHeap->SetName(L"CPU Descriptor Heap");
    m_Handle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    auto iter = m_SearchMap.emplace(numDescriptors, 0);
    m_AvailableDescriptors.emplace(0, iter);
}

void DescriptorPage::DiscardDescriptor(Descriptor& descriptor) {
    auto offset = (descriptor.handle.ptr - m_Handle.ptr) / m_IncrementSize;
    // Merge the descriptor to neighboring block
    // Since descriptors in vector is from front to back  in the page,
    // so descriptor will try merge forward firstly,
    // and then try merge bacwward.
    // That can reduce unnecessary construction on m_AvailableDescriptors.
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
        BlockOffset blockOffset = left->first;
        BlockSize   size        = left->second->first;
        if (blockOffset + size == offset) {
            m_SearchMap.erase(left->second);
            left->second = m_SearchMap.emplace(size + 1, blockOffset);
        }
    }
    // Merge backward
    if (right != m_AvailableDescriptors.end()) {
        BlockOffset blockOffset = right->first;
        BlockSize   size        = right->second->first;
        if (offset + 1 == blockOffset) {
            m_SearchMap.erase(right->second);
            m_AvailableDescriptors.erase(right);
            m_AvailableDescriptors.emplace(offset, m_SearchMap.emplace(size + 1, offset));
        }
    }
}

std::optional<std::vector<Descriptor>> DescriptorPage::Allocate(size_t numDescriptors) {
    auto iter = m_SearchMap.lower_bound(numDescriptors);
    if (iter == m_SearchMap.end()) return {};
    // release the block from the page.
    // search map
    BlockSize   size   = iter->first;
    BlockOffset offset = iter->second;
    m_SearchMap.erase(iter);
    m_AvailableDescriptors.erase(offset);

    // Now, we have the block info, offset and size, and then generator descriptors
    std::vector<Descriptor> ret;
    ret.reserve(numDescriptors);
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle;
    handle.InitOffsetted(m_Handle, offset, m_IncrementSize);

    for (size_t i = 0; i < numDescriptors; i++) {
        ret.emplace_back(handle, shared_from_this());
        handle.Offset(m_IncrementSize);
    }

    // Update the block info and insert to page and serach map
    offset += numDescriptors;
    size -= numDescriptors;

    iter = m_SearchMap.emplace(size, offset);
    m_AvailableDescriptors.emplace(offset, iter);

    return {std::move(ret)};
}

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptorPerPage)
    : m_Type(type), m_NumDescriptorPerPage(numDescriptorPerPage) {
}

void DescriptorAllocator::Initialize(ID3D12Device* device) {
    assert(device);
    m_Device = device;
}

Descriptor DescriptorAllocator::Allocate() {
    auto ret = Allocate(1);
    return std::move(ret[0]);
}

std::vector<Descriptor> DescriptorAllocator::Allocate(size_t numDescriptors) {
    // the Size larger than current page size, so update defualt page size,
    // and create large page
    m_NumDescriptorPerPage = std::max(m_NumDescriptorPerPage, numDescriptors);

    std::vector<Descriptor> ret;
    // Search page that have enough size to allocate descriptors.
    for (auto&& page : m_PagePool)
        if (auto descriptors = page->Allocate(numDescriptors))
            ret = std::move(descriptors.value());

    if (ret.empty()) {
        // Need create new page
        auto page = m_PagePool.emplace_front(
            DescriptorPage::Create(m_Device, m_Type, m_NumDescriptorPerPage));
        ret = std::move(page->Allocate(numDescriptors).value());
    }

    return ret;
}

}  // namespace Hitagi::Graphics::backend::DX12