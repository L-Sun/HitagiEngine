#pragma once
#include "CommandListManager.hpp"
#include "GpuResource.hpp"
#include "LinearAllocator.hpp"
#include "DynamicDescriptorHeap.hpp"
#include "PipeLineState.hpp"
#include "DepthBuffer.hpp"
#include "ColorBuffer.hpp"

namespace Hitagi::Graphics::DX12 {
class CommandContext {
    template <typename T1, typename T2>
    friend class FrameResource;
    friend class GpuBuffer;
    friend class TextureBuffer;
    friend class DynamicDescriptorHeap;

public:
    CommandContext(std::string_view name = "", D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
    CommandContext(const CommandContext&) = delete;
    CommandContext& operator=(const CommandContext&) = delete;
    ~CommandContext();

    ID3D12GraphicsCommandList5* GetCommandList() { return m_CommandList; }

    static void InitializeBuffer(GpuResource& dest, const void* data, size_t bufferSize);
    static void InitializeTexture(GpuResource& dest, const std::vector<D3D12_SUBRESOURCE_DATA>& subData);
    void        TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false);
    void        FlushResourceBarriers();

    void BuildRaytracingAccelerationStructure(ID3D12Resource*                                           resource,
                                              const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc);

    void ClearDepth(DepthBuffer& target);
    void ClearColor(ColorBuffer& target);

    void SetViewportAndScissor(const D3D12_VIEWPORT& viewport, const D3D12_RECT& rect);
    void SetViewport(const D3D12_VIEWPORT& viewport);
    void SetScissor(const D3D12_RECT& rect);

    void SetRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& RTVs);
    void SetRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& RTVs, D3D12_CPU_DESCRIPTOR_HANDLE DSV);
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets({RTV}); }
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) {
        SetRenderTargets({RTV}, DSV);
    }
    void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets({}, DSV); }

    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap);
    void SetDescriptorHeaps(const std::vector<D3D12_DESCRIPTOR_HEAP_TYPE>& type, std::vector<ID3D12DescriptorHeap*>& heaps);
    void SetRootSignature(const RootSignature& rootSignature);
    void BindDescriptorHeaps();
    void SetConstant(unsigned rootIndex, uint32_t constant, unsigned offset = 0);
    void SetDynamicDescriptor(unsigned rootIndex, unsigned offset, D3D12_CPU_DESCRIPTOR_HANDLE handle);
    void SetDynamicSampler(unsigned rootIndex, unsigned offset, D3D12_CPU_DESCRIPTOR_HANDLE handle);

    void SetPipeLineState(const PipeLineState& pso);
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
    void SetVertexBuffer(unsigned slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
    void SetVertexBuffers(unsigned startSlot, const std::vector<D3D12_VERTEX_BUFFER_VIEW>& VBViews);

    void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
    void DrawInstanced(unsigned VertexCountPerInstance, unsigned InstanceCount, unsigned StartVertexLocation, unsigned StartInstanceLocation);
    void DrawIndexedInstanced(unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, int baseVertexLocation, unsigned startInstanceLocation);

    uint64_t Finish(bool waitForComplete = false);
    void     Reset();

private:
    std::string                 m_Name;
    ID3D12GraphicsCommandList5* m_CommandList      = nullptr;
    ID3D12CommandAllocator*     m_CommandAllocator = nullptr;

    std::array<std::unique_ptr<DynamicDescriptorHeap>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_DynamicDescriptorHeaps;
    std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>                  m_CurrentDescriptorHeaps;

    ID3D12RootSignature* m_CurrRootSignature = nullptr;

    std::array<D3D12_RESOURCE_BARRIER, 16> m_Barriers;
    unsigned                               m_NumBarriersToFlush = 0;

    LinearAllocator m_CpuLinearAllocator;
    LinearAllocator m_GpuLinearAllocator;

    D3D12_COMMAND_LIST_TYPE m_Type;
};
}  // namespace Hitagi::Graphics::DX12