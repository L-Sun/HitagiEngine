#include "CommandAllocatorPool.hpp"

namespace Hitagi::Graphics::DX12 {

CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type) : m_Type(type), m_Device(nullptr) {}
CommandAllocatorPool::~CommandAllocatorPool() {}

void CommandAllocatorPool::Initialize(ID3D12Device6* device) { m_Device = device; }

ID3D12CommandAllocator* CommandAllocatorPool::GetAllocator(uint64_t completedFenceValue) {
    if (!m_ReadyAllocators.empty()) {
        auto& [fenceValue, allocator] = m_ReadyAllocators.front();
        if (fenceValue <= completedFenceValue) {
            ThrowIfFailed(allocator->Reset());
            m_ReadyAllocators.pop();
            return allocator;
        }
    }
    // If no allocator(s) were ready to be used, create a new one
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> newAllocator;
    if (newAllocator == nullptr) {
        ThrowIfFailed(m_Device->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&newAllocator)));
        m_AllocatorPool.push_back(newAllocator);
    }
    return newAllocator.Get();
}

void CommandAllocatorPool::DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator) {
    m_ReadyAllocators.push({fenceValue, allocator});
}
}  // namespace Hitagi::Graphics::DX12