#include "CommandContext.hpp"
#include "DX12DriverAPI.hpp"
#include "GpuBuffer.hpp"
#include "Sampler.hpp"
#include "EnumConverter.hpp"

namespace Hitagi::Graphics::backend::DX12 {

CommandContext::CommandContext(DX12DriverAPI& driver, D3D12_COMMAND_LIST_TYPE type)
    : m_Driver(driver),
      m_CpuLinearAllocator(driver.GetDevice(), AllocationPageType::CPU_WRITABLE, [&driver](uint64_t fence) { return driver.GetCmdMgr().IsFenceComplete(fence); }),
      m_GpuLinearAllocator(driver.GetDevice(), AllocationPageType::GPU_EXCLUSIVE, [&driver](uint64_t fence) { return driver.GetCmdMgr().IsFenceComplete(fence); }),
      m_DynamicViewDescriptorHeap(driver.GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, [&driver](uint64_t fence) { return driver.GetCmdMgr().IsFenceComplete(fence); }),
      m_DynamicSamplerDescriptorHeap(driver.GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, [&driver](uint64_t fence) { return driver.GetCmdMgr().IsFenceComplete(fence); }),
      m_Type(type) {
    driver.GetCmdMgr().CreateNewCommandList(m_Type, &m_CommandList, &m_CommandAllocator);
}
CommandContext::~CommandContext() {
    if (m_CommandList) m_CommandList->Release();
    // Finish() is not called before destruction. so use the last completed fence
    if (m_CommandAllocator) {
        auto&    queue      = m_Driver.GetCmdMgr().GetQueue(m_Type);
        uint64_t fenceValue = queue.GetLastCompletedFenceValue();
        queue.DiscardAllocator(fenceValue, m_CommandAllocator);
    }
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

void CommandContext::BindDescriptorHeaps() {
    unsigned              nonNullHeaps = 0;
    ID3D12DescriptorHeap* heapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    for (unsigned i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        ID3D12DescriptorHeap* heap = m_CurrentDescriptorHeaps[i];
        if (heap != nullptr)
            heapsToBind[nonNullHeaps++] = heap;
    }

    if (nonNullHeaps > 0) m_CommandList->SetDescriptorHeaps(nonNullHeaps, heapsToBind);
}

uint64_t CommandContext::Finish(bool waitForComplete) {
    FlushResourceBarriers();

    CommandQueue& queue = m_Driver.GetCmdMgr().GetQueue(m_Type);

    uint64_t fenceValue = queue.ExecuteCommandList(m_CommandList);
    m_CpuLinearAllocator.SetFence(fenceValue);
    m_GpuLinearAllocator.SetFence(fenceValue);
    queue.DiscardAllocator(fenceValue, m_CommandAllocator);
    m_CommandAllocator = nullptr;

    m_DynamicViewDescriptorHeap.Reset(fenceValue);
    m_DynamicSamplerDescriptorHeap.Reset(fenceValue);
    m_CurrentDescriptorHeaps = {};

    if (waitForComplete) m_Driver.GetCmdMgr().WaitForFence(fenceValue);

    return fenceValue;
}

void CommandContext::Reset() {
    // We only call Reset() on previously freed contexts.  The command list persists, but we must
    // request a new allocator.
    assert(m_CommandList != nullptr && m_CommandAllocator == nullptr);
    m_CommandAllocator = m_Driver.GetCmdMgr().GetQueue(m_Type).RequestAllocator();
    m_CommandList->Reset(m_CommandAllocator, nullptr);

    m_NumBarriersToFlush = 0;
}

void GraphicsCommandContext::SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    auto vp   = CD3DX12_VIEWPORT(x, y, width, height);
    auto rect = CD3DX12_RECT(x, y, width, height);
    m_CommandList->RSSetViewports(1, &vp);
    m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsCommandContext::SetRenderTarget(Graphics::RenderTarget& rt) {
    auto& renderTarget = *rt.GetBackend<RenderTarget>();
    TransitionResource(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->OMSetRenderTargets(1, &renderTarget.GetRTV().handle, false, nullptr);
}

void GraphicsCommandContext::SetRenderTargetAndDepthBuffer(Graphics::RenderTarget& rt, Graphics::DepthBuffer& depthBuffer) {
    auto& renderTarget = *rt.GetBackend<RenderTarget>();
    auto& db           = *depthBuffer.GetBackend<DepthBuffer>();
    TransitionResource(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->OMSetRenderTargets(1, &renderTarget.GetRTV().handle, false, &db.GetDSV().handle);
}

void GraphicsCommandContext::ClearRenderTarget(Graphics::RenderTarget& rt) {
    auto& renderTarget = *rt.GetBackend<RenderTarget>();
    TransitionResource(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->ClearRenderTargetView(renderTarget.GetRTV().handle, vec4f(0, 0, 0, 1), 0, nullptr);
}

void GraphicsCommandContext::ClearDepthBuffer(Graphics::DepthBuffer& depthBuffer) {
    auto& db = *depthBuffer.GetBackend<DepthBuffer>();
    TransitionResource(db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->ClearDepthStencilView(db.GetDSV().handle, D3D12_CLEAR_FLAG_DEPTH, db.GetClearDepth(), db.GetClearStencil(), 0, nullptr);
}

void GraphicsCommandContext::SetPipelineState(const Graphics::PipelineState& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;
    SetPSO(*pipeline.GetBackend<PSO>());
    SetRootSignature(*pipeline.GetRootSignature()->GetBackend<RootSignature>());
}

void GraphicsCommandContext::SetParameter(std::string_view name, const Graphics::ConstantBuffer& cb, size_t offset) {
    auto sig                      = m_Pipeline->GetRootSignature()->GetBackend<RootSignature>();
    auto [rootIndex, tableOffset] = sig->GetParameterTable(name);
    SetDynamicDescriptor(rootIndex, tableOffset, cb.GetBackend<ConstantBuffer>()->GetCBV(offset));
}

void GraphicsCommandContext::SetParameter(std::string_view name, const Graphics::TextureBuffer& texture) {
    auto sig                      = m_Pipeline->GetRootSignature()->GetBackend<RootSignature>();
    auto [rootIndex, tableOffset] = sig->GetParameterTable(name);
    SetDynamicDescriptor(rootIndex, tableOffset, texture.GetBackend<TextureBuffer>()->GetSRV());
}

void GraphicsCommandContext::SetParameter(std::string_view name, const Graphics::Sampler& sampler) {
    auto sig                      = m_Pipeline->GetRootSignature()->GetBackend<RootSignature>();
    auto [rootIndex, tableOffset] = sig->GetParameterTable(name);
    SetDynamicSampler(rootIndex, tableOffset, sampler.GetBackend<Sampler>()->GetDescriptor());
}

void GraphicsCommandContext::Present(Graphics::RenderTarget& rt) {
    auto& renderTarget = *rt.GetBackend<RenderTarget>();
    TransitionResource(renderTarget, D3D12_RESOURCE_STATE_PRESENT, true);
}

void GraphicsCommandContext::Draw(const Graphics::MeshBuffer& mesh) {
    auto  indexBuffer = mesh.indices->GetBackend<IndexBuffer>();
    auto& layout      = m_Pipeline->GetInputLayout();

    auto ibv = indexBuffer->IndexBufferView();
    m_CommandList->IASetIndexBuffer(&ibv);
    for (auto&& [semanticName, vertex] : mesh.vertices) {
        auto iter = layout.begin();
        while (iter != layout.end() && iter->semanticName != semanticName) iter++;
        if (iter == layout.end()) continue;

        auto vbv = vertex->GetBackend<VertexBuffer>()->VertexBufferView();
        m_CommandList->IASetVertexBuffers(iter->inputSlot, 1, &vbv);
    }

    FlushResourceBarriers();
    m_DynamicViewDescriptorHeap.CommitStagedDescriptors(*this, &ID3D12GraphicsCommandList5::SetGraphicsRootDescriptorTable);
    m_DynamicSamplerDescriptorHeap.CommitStagedDescriptors(*this, &ID3D12GraphicsCommandList5::SetGraphicsRootDescriptorTable);
    m_CommandList->IASetPrimitiveTopology(ToDxTopology(mesh.primitive));
    m_CommandList->DrawIndexedInstanced(indexBuffer->GetElementCount(), 1, 0, 0, 0);
}

void CopyCommandContext::InitializeBuffer(GpuResource& dest, const uint8_t* data, size_t dataSize) {
    auto uploadBuffer = m_CpuLinearAllocator.Allocate(dataSize);
    std::copy_n(data, dataSize, uploadBuffer.cpuPtr);

    TransitionResource(dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    m_CommandList->CopyBufferRegion(dest.GetResource(), 0, uploadBuffer.pageFrom.lock()->GetResource(),
                                    uploadBuffer.pageOffset, dataSize);
    TransitionResource(dest, D3D12_RESOURCE_STATE_COMMON, true);

    Finish(true);
}

void CopyCommandContext::InitializeTexture(GpuResource& dest, const std::vector<D3D12_SUBRESOURCE_DATA>& subData) {
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(dest.GetResource(), 0, subData.size());
    auto         uploadBuffer     = m_CpuLinearAllocator.Allocate(uploadBufferSize);

    UpdateSubresources(m_CommandList, dest.GetResource(), uploadBuffer.pageFrom.lock()->GetResource(),
                       uploadBuffer.pageOffset, 0, subData.size(), const_cast<D3D12_SUBRESOURCE_DATA*>(subData.data()));
    TransitionResource(dest, D3D12_RESOURCE_STATE_COMMON);

    Finish(true);
}

}  // namespace Hitagi::Graphics::backend::DX12