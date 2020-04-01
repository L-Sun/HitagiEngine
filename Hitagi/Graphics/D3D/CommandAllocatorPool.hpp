#pragma once
#include <vector>
#include <queue>
#include "D3Dpch.hpp"

namespace Hitagi::Graphics {
class CommandAllocatorPool {
public:
    CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type);
    ~CommandAllocatorPool();

    void Initialize(ID3D12Device6* device);
    void Finalize();

    ID3D12CommandAllocator* GetAllocator(uint64_t completedFenceValue);
    void                    DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator);
    inline bool             Size() const { return m_AllocatorPool.size(); }

private:
    const D3D12_COMMAND_LIST_TYPE                            m_type;
    ID3D12Device6*                                           m_Device;
    std::vector<ID3D12CommandAllocator*>                     m_AllocatorPool;
    std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>> m_readyAllocators;
};
}  // namespace Hitagi::Graphics