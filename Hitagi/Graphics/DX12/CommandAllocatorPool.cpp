#include "CommandAllocatorPool.hpp"

namespace hitagi::graphics::backend::DX12 {

CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type) : m_Type(type) {}

ID3D12CommandAllocator* CommandAllocatorPool::GetAllocator(uint64_t completed_fence_value) {
    if (!m_ReadyAllocators.empty()) {
        auto& [fenceValue, allocator] = m_ReadyAllocators.front();
        if (fenceValue <= completed_fence_value) {
            ThrowIfFailed(allocator->Reset());
            m_ReadyAllocators.pop();
            return allocator;
        }
    }
    // If no allocator(s) were ready to be used, create a new one
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> new_allocator;
    if (new_allocator == nullptr) {
        ThrowIfFailed(m_Device->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&new_allocator)));
        m_AllocatorPool.push_back(new_allocator);
    }
    return new_allocator.Get();
}

void CommandAllocatorPool::DiscardAllocator(uint64_t fence_value, ID3D12CommandAllocator* allocator) {
    m_ReadyAllocators.push({fence_value, allocator});
}
}  // namespace hitagi::graphics::backend::DX12