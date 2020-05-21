#include "CommandContext.hpp"
#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {

CommandContext::CommandContext(std::string_view name, D3D12_COMMAND_LIST_TYPE type)
    : m_Name(name),

      m_CpuLinearAllocator(LinearAllocatorType::CPU_WRITABLE),
      m_GpuLinearAllocator(LinearAllocatorType::GPU_EXCLUSIVE),
      m_Type(type) {
    g_CommandManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CommandAllocator);

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
        m_DynamicDescriptorHeaps[i] =
            std::make_unique<DynamicDescriptorHeap>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
        m_CurrentDescriptorHeaps[i] = nullptr;
    }
}
CommandContext::~CommandContext() {
    if (m_CommandList) m_CommandList->Release();
    if (m_CommandAllocator) {
        auto&    queue      = g_CommandManager.GetQueue(m_Type);
        uint64_t fenceValue = queue.GetLastCompletedFenceValue();
        m_CpuLinearAllocator.DiscardPages(fenceValue);
        m_GpuLinearAllocator.DiscardPages(fenceValue);
        queue.DiscardAllocator(fenceValue, m_CommandAllocator);
    }
}

void CommandContext::InitializeBuffer(GpuResource& dest, const void* data, size_t bufferSize) {
    CommandContext context;

    auto uploadBuffer = context.m_CpuLinearAllocator.Allocate(bufferSize);
    std::memcpy(uploadBuffer.CpuPtr, data, bufferSize);

    context.TransitionResource(dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    context.m_CommandList->CopyBufferRegion(dest.GetResource(), 0, uploadBuffer.resource.GetResource(),
                                            uploadBuffer.offset, bufferSize);
    context.TransitionResource(dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

    context.Finish(true);
}

void CommandContext::InitializeTexture(GpuResource& dest, const std::vector<D3D12_SUBRESOURCE_DATA>& subData) {
    CommandContext context;

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(dest.GetResource(), 0, subData.size());
    auto         uploadBuffer     = context.m_CpuLinearAllocator.Allocate(uploadBufferSize);

    UpdateSubresources(context.m_CommandList, dest.GetResource(), uploadBuffer.resource.GetResource(),
                       uploadBuffer.offset, 0, subData.size(), const_cast<D3D12_SUBRESOURCE_DATA*>(subData.data()));
    context.TransitionResource(dest, D3D12_RESOURCE_STATE_GENERIC_READ);

    context.Finish(true);
}

void CommandContext::BuildRaytracingAccelerationStructure(
    ID3D12Resource* resource, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc) {
    m_CommandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    assert(m_NumBarriersToFlush < 16 && "Exceeded arbitrary limit on buffered barriers");
    D3D12_RESOURCE_BARRIER& uavBarrier = m_Barriers[m_NumBarriersToFlush++];
    uavBarrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource           = resource;
    uavBarrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    FlushResourceBarriers();
}

void CommandContext::TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool flushImmediate) {
    D3D12_RESOURCE_STATES oldState = resource.m_UsageState;
    if (oldState != newState) {
        assert(m_NumBarriersToFlush < 16 && "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& barrierDesc = m_Barriers[m_NumBarriersToFlush++];
        barrierDesc.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierDesc.Transition.pResource    = resource.GetResource();
        barrierDesc.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrierDesc.Transition.StateBefore  = oldState;
        barrierDesc.Transition.StateAfter   = newState;

        if (newState == resource.m_TransitioningState) {
            barrierDesc.Flags             = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            resource.m_TransitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
        } else {
            barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        }
        resource.m_UsageState = newState;
    }

    if (flushImmediate || m_NumBarriersToFlush == 16) FlushResourceBarriers();
}

void CommandContext::FlushResourceBarriers() {
    if (m_NumBarriersToFlush > 0) {
        m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_Barriers.data());
        m_NumBarriersToFlush = 0;
    }
}

void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap) {
    if (m_CurrentDescriptorHeaps[type] != heap) {
        m_CurrentDescriptorHeaps[type] = heap;
        BindDescriptorHeaps();
    }
}

void CommandContext::SetDescriptorHeaps(const std::vector<D3D12_DESCRIPTOR_HEAP_TYPE>& type,
                                        std::vector<ID3D12DescriptorHeap*>&            heaps) {
    assert(type.size() == heaps.size());
    bool changed = false;
    for (unsigned i = 0; i < type.size(); i++) {
        if (m_CurrentDescriptorHeaps[type[i]] != heaps[i]) m_CurrentDescriptorHeaps[type[i]] = heaps[i];
        changed = true;
    }
    if (changed) BindDescriptorHeaps();
}

void CommandContext::BindDescriptorHeaps() {
    unsigned                                                                NonNullHeaps = 0;
    std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> HeapsToBind;
    for (unsigned i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        ID3D12DescriptorHeap* HeapIter = m_CurrentDescriptorHeaps[i];
        if (HeapIter != nullptr) HeapsToBind[NonNullHeaps++] = HeapIter;
    }

    if (NonNullHeaps > 0) m_CommandList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind.data());
}

void CommandContext::SetRootSignature(const RootSignature& rootSignature) {
    auto signature = rootSignature.GetRootSignature();

    if (m_CurrRootSignature == signature) return;

    m_CurrRootSignature = signature;

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
        m_DynamicDescriptorHeaps[i]->ParseRootSignature(rootSignature);
    }

    m_CommandList->SetGraphicsRootSignature(m_CurrRootSignature);
}

void CommandContext::SetDynamicDescriptor(unsigned rootIndex, unsigned offset,
                                          D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptor(rootIndex, offset, 1, handle);
}

void CommandContext::SetDynamicSampler(unsigned rootIndex, unsigned offset, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->StageDescriptor(rootIndex, offset, 1, handle);
}

void CommandContext::ClearDepth(DepthBuffer& target) {
    m_CommandList->ClearDepthStencilView(target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, target.GetClearDepth(), target.GetClearStencil(), 0, nullptr);
}
void CommandContext::ClearColor(ColorBuffer& target) {
    m_CommandList->ClearRenderTargetView(target.GetRTV(), target.GetClearColor(), 0, nullptr);
}

void CommandContext::SetViewportAndScissor(const D3D12_VIEWPORT& viewport, const D3D12_RECT& rect) {
    SetViewport(viewport);
    SetScissor(rect);
}

void CommandContext::SetViewport(const D3D12_VIEWPORT& viewport) { m_CommandList->RSSetViewports(1, &viewport); }

void CommandContext::SetScissor(const D3D12_RECT& rect) { m_CommandList->RSSetScissorRects(1, &rect); }

void CommandContext::SetRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& RTVs,
                                      D3D12_CPU_DESCRIPTOR_HANDLE                     DSV) {
    m_CommandList->OMSetRenderTargets(RTVs.size(), RTVs.data(), false, &DSV);
}

void CommandContext::SetRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& RTVs) {
    m_CommandList->OMSetRenderTargets(RTVs.size(), RTVs.data(), false, nullptr);
}

void CommandContext::SetPipeLineState(const PipeLineState& pso) { m_CommandList->SetPipelineState(pso.GetPSO()); }

void CommandContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView) { m_CommandList->IASetIndexBuffer(&IBView); }

void CommandContext::SetVertexBuffer(unsigned slot, const D3D12_VERTEX_BUFFER_VIEW& VBView) {
    m_CommandList->IASetVertexBuffers(slot, 1, &VBView);
}

void CommandContext::SetVertexBuffers(unsigned startSlot, const std::vector<D3D12_VERTEX_BUFFER_VIEW>& VBViews) {
    m_CommandList->IASetVertexBuffers(startSlot, VBViews.size(), VBViews.data());
}

void CommandContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology) {
    m_CommandList->IASetPrimitiveTopology(topology);
}

void CommandContext::DrawIndexedInstanced(unsigned indexCountPerInstance, unsigned instanceCount,
                                          unsigned startIndexLocation, int baseVertexLocation,
                                          unsigned startInstanceLocation) {
    FlushResourceBarriers();
    for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
        m_DynamicDescriptorHeaps[i]->CommitStagedDescriptors(
            *this, &ID3D12GraphicsCommandList5::SetGraphicsRootDescriptorTable);
    }
    m_CommandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation,
                                        startInstanceLocation);
}

uint64_t CommandContext::Finish(bool waitForComplete) {
    assert(m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT || m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE);
    FlushResourceBarriers();

    CommandQueue& queue = g_CommandManager.GetQueue(m_Type);

    uint64_t fenceValue = queue.ExecuteCommandList(m_CommandList);
    m_CpuLinearAllocator.DiscardPages(fenceValue);
    m_GpuLinearAllocator.DiscardPages(fenceValue);
    queue.DiscardAllocator(fenceValue, m_CommandAllocator);
    m_CommandAllocator = nullptr;

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
        m_DynamicDescriptorHeaps[i]->Reset(fenceValue);
        m_CurrentDescriptorHeaps[i] = nullptr;
    }

    if (waitForComplete) g_CommandManager.WaitForFence(fenceValue);

    return fenceValue;
}

void CommandContext::Reset() {
    // We only call Reset() on previously freed contexts.  The command list persists, but we must
    // request a new allocator.
    assert(m_CommandList != nullptr && m_CommandAllocator == nullptr);
    m_CommandAllocator = g_CommandManager.GetQueue(m_Type).RequestAllocator();
    m_CommandList->Reset(m_CommandAllocator, nullptr);

    m_CurrRootSignature  = nullptr;
    m_NumBarriersToFlush = 0;

    BindDescriptorHeaps();
}

}  // namespace Hitagi::Graphics::DX12