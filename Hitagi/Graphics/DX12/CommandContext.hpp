#pragma once
#include "CommandListManager.hpp"
#include "GpuResource.hpp"
#include "LinearAllocator.hpp"
#include "DynamicDescriptorHeap.hpp"
#include "PipeLineState.hpp"

namespace Hitagi::Graphics::DX12 {
class CommandContext {
    template <typename T1, typename T2>
    friend class FrameResource;
    friend class GpuBuffer;
    friend class TextureBuffer;
    friend class DynamicDescriptorHeap;

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

    void SetViewportAndScissor(const D3D12_VIEWPORT& viewport, const D3D12_RECT& rect);
    void SetViewport(const D3D12_VIEWPORT& viewport);
    void SetScissor(const D3D12_RECT& rect);

    void SetRenderTargets(unsigned numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[]);
    void SetRenderTargets(unsigned numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets(1, &RTV); }
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) {
        SetRenderTargets(1, &RTV, DSV);
    }
    void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(0, nullptr, DSV); }

    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap);
    void SetDescriptorHeaps(unsigned heapCount, D3D12_DESCRIPTOR_HEAP_TYPE type[], ID3D12DescriptorHeap* heaps[]);
    void SetRootSignature(const RootSignature& rootSignature);
    void BindDescriptorHeaps();
    void SetDynamicDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned rootIndex, unsigned offset,
                              D3D12_CPU_DESCRIPTOR_HANDLE handle);

    void SetPipeLineState(const PipeLineState& pso);
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
    void SetVertexBuffer(unsigned slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
    void SetVertexBuffers(unsigned startSlot, unsigned count, const D3D12_VERTEX_BUFFER_VIEW VBViews[]);

    void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);

    void DrawIndexedInstanced(unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation,
                              int baseVertexLocation, unsigned startInstanceLocation);

    uint64_t Finish(bool waitForComplete = false);
    void     Reset();

private:
    CommandListManager&         m_CommandManager;
    ID3D12GraphicsCommandList5* m_CommandList;
    ID3D12CommandAllocator*     m_CommandAllocator;

    std::unique_ptr<DynamicDescriptorHeap> m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    ID3D12DescriptorHeap*                  m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    ID3D12RootSignature* m_CurrRootSignature;

    D3D12_RESOURCE_BARRIER m_Barriers[16];
    unsigned               m_NumBarriersToFlush;

    LinearAllocator m_CpuLinearAllocator;
    LinearAllocator m_GpuLinearAllocator;

    D3D12_COMMAND_LIST_TYPE m_Type;
};
}  // namespace Hitagi::Graphics::DX12