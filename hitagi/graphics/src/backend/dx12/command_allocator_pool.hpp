#pragma once
#include "d3d_pch.hpp"

namespace hitagi::gfx::backend::DX12 {
class CommandAllocatorPool {
public:
    CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type);

    void Initialize(ID3D12Device* device) {
        assert(device);
        m_Device = device;
    }

    ID3D12CommandAllocator* GetAllocator(uint64_t completed_fence_value);
    void                    DiscardAllocator(uint64_t fence_value, ID3D12CommandAllocator* allocator);
    inline bool             Size() const noexcept { return m_AllocatorPool.size(); }

private:
    std::mutex                                                       m_Mutex;
    ID3D12Device*                                                    m_Device = nullptr;
    D3D12_COMMAND_LIST_TYPE                                          m_Type;
    std::pmr::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_AllocatorPool;
    std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>,
               std::pmr::deque<std::pair<uint64_t, ID3D12CommandAllocator*>>>
        m_ReadyAllocators;
};
}  // namespace hitagi::gfx::backend::DX12