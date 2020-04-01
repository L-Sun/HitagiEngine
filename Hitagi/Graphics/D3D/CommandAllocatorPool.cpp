#include <cassert>
#include <sstream>
#include "CommandAllocatorPool.hpp"

namespace Hitagi::Graphics {

CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type) : m_type(type), m_Device(nullptr) {}
CommandAllocatorPool::~CommandAllocatorPool() { Finalize(); }

void CommandAllocatorPool::Initialize(ID3D12Device6* device) { m_Device = device; }

void CommandAllocatorPool::Finalize() {
    for (auto&& allocator : m_AllocatorPool) allocator->Release();
    m_AllocatorPool.clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::GetAllocator(uint64_t completedFenceValue) {
    ID3D12CommandAllocator* allocator = nullptr;
    if (!m_readyAllocators.empty()) {
        auto& [fenceValue, allocator] = m_readyAllocators.front();
        if (fenceValue <= completedFenceValue) {
            allocator = allocator;
            ThrowIfFailed(allocator->Reset());
            m_readyAllocators.pop();
        }
    }
    // If no allocator(s) were ready to be used, create a new one
    if (allocator == nullptr) {
        ThrowIfFailed(m_Device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&allocator)));
        std::wstringstream wss;
        wss << L"CommandAllocator " << m_AllocatorPool.size();
        allocator->SetName(wss.str().c_str());
        m_AllocatorPool.push_back(allocator);
    }
    return allocator;
}

void CommandAllocatorPool::DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator) {
    m_readyAllocators.push({fenceValue, allocator});
}
}  // namespace Hitagi::Graphics