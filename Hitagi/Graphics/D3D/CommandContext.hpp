#pragma once
#include "D3Dpch.hpp"
#include "CommandListManager.hpp"
#include "GpuResource.hpp"
#include "LinearAllocator.hpp"
#include "DynamicDescriptorHeap.hpp"

namespace Hitagi::Graphics {
class CommandContext {
    template <typename T1, typename T2>
    friend class FrameResource;
    friend class GpuBuffer;
    friend class TextureBuffer;

public:
    CommandContext(CommandListManager& cmdManager, D3D12_COMMAND_LIST_TYPE type);
    CommandContext(const CommandContext&) = delete;
    CommandContext& operator=(const CommandContext&) = delete;
    ~CommandContext();

    ID3D12GraphicsCommandList5* GetCommandList() { return m_CommandList; }

    void InitializeBuffer(GpuResource& dest, const void* data, size_t bufferSize);
    void InitializeTexture(GpuResource& dest, size_t numSubresources, D3D12_SUBRESOURCE_DATA subData[]);
    void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false);
    void FlushResourceBarriers();

    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap);
    void SetDescriptorHeaps(unsigned heapCount, D3D12_DESCRIPTOR_HEAP_TYPE type[], ID3D12DescriptorHeap* heaps[]);
    void SetRootSignature(const RootSignature& rootSignature);
    void BindDescriptorHeaps();
    void SetDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned rootIndex, unsigned offset,
                              D3D12_CPU_DESCRIPTOR_HANDLE handle);

    void DrawIndexedInstanced(unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation,
                              int baseVertexLocation, unsigned startInstanceLocation);

    uint64_t Finish(bool waitForComplete = false);
    void     Reset();

private:
    ID3D12Device6*              m_Device;
    CommandListManager&         m_CommandManager;
    ID3D12GraphicsCommandList5* m_CommandList;
    ID3D12CommandAllocator*     m_CommandAllocator;

    std::unique_ptr<DynamicDescriptorHeap> m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    ID3D12DescriptorHeap*                  m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    ID3D12RootSignature* m_CurrRootSignature;

    D3D12_RESOURCE_BARRIER m_Barriers[16];
    unsigned               m_NumBarriersToFlush;

    LinearAllocator m_LinearAllocator;

    D3D12_COMMAND_LIST_TYPE m_Type;
};
}  // namespace Hitagi::Graphics