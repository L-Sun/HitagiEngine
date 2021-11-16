#include "DescriptorAllocator.hpp"

namespace Hitagi::Graphics::backend::DX12 {
Descriptor::Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::shared_ptr<DescriptorPage> page_from)
    : handle(std::move(handle)), page_from(std::move(page_from)) {}

Descriptor::~Descriptor() {
    if (page_from) page_from->DiscardDescriptor(*this);
}

DescriptorPage::DescriptorPage(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptors) {
    assert(device);
    m_IncrementSize = device->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc;
    desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // Just CPU Visiable
    desc.NodeMask       = 0;
    desc.NumDescriptors = num_descriptors;
    desc.Type           = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));
    m_DescriptorHeap->SetName(L"CPU Descriptor Heap");
    m_Handle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    auto iter = m_SearchMap.emplace(num_descriptors, 0);
    m_AvailableDescriptors.emplace(0, iter);
}

void DescriptorPage::DiscardDescriptor(Descriptor& descriptor) {
    auto   offset = (descriptor.handle.ptr - m_Handle.ptr) / m_IncrementSize;
    size_t size   = 1;
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

std::optional<std::vector<Descriptor>> DescriptorPage::Allocate(size_t num_descriptors) {
    auto iter = m_SearchMap.lower_bound(num_descriptors);
    if (iter == m_SearchMap.end()) return {};

    // update block
    auto        block_info = m_SearchMap.extract(iter);
    BlockOffset offset     = block_info.mapped();
    auto        block      = m_AvailableDescriptors.extract(offset);

    // Now, we have the block info, offset and size, and then generate descriptors from the back of block
    std::vector<Descriptor>       result;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle;
    handle.InitOffsetted(m_Handle, offset, m_IncrementSize);

    for (size_t i = 0; i < num_descriptors; i++) {
        result.emplace_back(handle, shared_from_this());
        handle.Offset(m_IncrementSize);
    }

    block_info.key() -= num_descriptors;
    if (block_info.key() != 0) {
        block_info.mapped() = offset + num_descriptors;
        block.key()         = block_info.mapped();
        block.mapped()      = m_SearchMap.insert(std::move(block_info));
        m_AvailableDescriptors.insert(std::move(block));
    }

    return {std::move(result)};
}

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t num_descriptor_per_page)
    : m_Type(type), m_NumDescriptorPerPage(num_descriptor_per_page) {
}

void DescriptorAllocator::Initialize(ID3D12Device* device) {
    assert(device);
    m_Device = device;
}

Descriptor DescriptorAllocator::Allocate() {
    auto ret = Allocate(1);
    return std::move(ret[0]);
}

std::vector<Descriptor> DescriptorAllocator::Allocate(size_t num_descriptors) {
    // the Size larger than current page size, so update defualt page size,
    // and create large page
    m_NumDescriptorPerPage = std::max(m_NumDescriptorPerPage, num_descriptors);

    std::vector<Descriptor> ret;
    // Search page that have enough size to allocate descriptors.
    for (auto&& page : m_PagePool)
        if (auto descriptors = page->Allocate(num_descriptors))
            ret = std::move(descriptors.value());

    if (ret.empty()) {
        // Need create new page
        auto page = m_PagePool.emplace_front(
            DescriptorPage::Create(m_Device, m_Type, m_NumDescriptorPerPage));
        ret = std::move(page->Allocate(num_descriptors).value());
    }

    return ret;
}

}  // namespace Hitagi::Graphics::backend::DX12