#pragma once
#include "command_allocator_pool.hpp"

namespace hitagi::gfx::backend::DX12 {
class CommandQueue {
    friend class CommandContext;
    friend class CommandListManager;

public:
    CommandQueue(D3D12_COMMAND_LIST_TYPE type);
    ~CommandQueue();

    inline bool IsReady() const { return m_CommandQueue != nullptr; }

    void Initialize(ID3D12Device6* device);

    ID3D12CommandAllocator* RequestAllocator();
    void                    DiscardAllocator(uint64_t fence_value, ID3D12CommandAllocator* allocator);

    uint64_t ExecuteCommandList(ID3D12CommandList* list);
    bool     IsFenceComplete(uint64_t fence_value);
    uint64_t IncreaseFence();
    void     WaitForQueue(CommandQueue& other);
    void     WaitForFence(uint64_t fence_value);
    void     WaitIdle() { WaitForFence(IncreaseFence()); }

    ID3D12CommandQueue* GetCommandQueue() { return m_CommandQueue.Get(); }
    uint64_t            GetNextFenceValue() const { return m_NextFenceValue; }
    uint64_t            GetLastCompletedFenceValue() const { return m_LastCompletedFenceValue; }

private:
    D3D12_COMMAND_LIST_TYPE                    m_Type;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
    CommandAllocatorPool                       m_AllocatorPool;

    // Fence
    Microsoft::WRL::ComPtr<ID3D12Fence1> m_Fence;
    uint64_t                             m_LastCompletedFenceValue;
    uint64_t                             m_NextFenceValue;
    HANDLE                               m_FenceHandle{};
};

class CommandListManager {
public:
    CommandListManager();

    void Initialize(ID3D12Device6* device);

    void CreateNewCommandList(std::string_view name, D3D12_COMMAND_LIST_TYPE type, ID3D12GraphicsCommandList5** list,
                              ID3D12CommandAllocator** allocator);
    void WaitForFence(uint64_t fence_value);
    bool IsFenceComplete(uint64_t fence_value) {
        return GetQueue(static_cast<D3D12_COMMAND_LIST_TYPE>(fence_value >> 56)).IsFenceComplete(fence_value);
    }

    CommandQueue& GetGraphicsQueue() { return m_GraphicsQueue; }
    CommandQueue& GetComputeQueue() { return m_ComputeQueue; }
    CommandQueue& GetCopyQueue() { return m_CopyQueue; }

    CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) {
        switch (type) {
            case D3D12_COMMAND_LIST_TYPE_COMPUTE:
                return m_ComputeQueue;
            case D3D12_COMMAND_LIST_TYPE_COPY:
                return m_CopyQueue;
            default:
                return m_GraphicsQueue;
        }
    }

    ID3D12CommandQueue* GetCommandQueue() { return m_GraphicsQueue.GetCommandQueue(); }

    void IdleGPU() {
        m_GraphicsQueue.WaitIdle();
        m_ComputeQueue.WaitIdle();
        m_CopyQueue.WaitIdle();
    }

    ID3D12Device6* GetDevice() const noexcept { return m_Device; }

private:
    ID3D12Device6* m_Device = nullptr;
    CommandQueue   m_GraphicsQueue;
    CommandQueue   m_ComputeQueue;
    CommandQueue   m_CopyQueue;
};
}  // namespace hitagi::gfx::backend::DX12