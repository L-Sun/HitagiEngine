#include "CommandContext.hpp"
#include "DX12DriverAPI.hpp"
#include "GpuBuffer.hpp"
#include "Sampler.hpp"
#include "Utils.hpp"

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
        auto&    queue       = m_Driver.GetCmdMgr().GetQueue(m_Type);
        uint64_t fence_value = queue.GetLastCompletedFenceValue();
        queue.DiscardAllocator(fence_value, m_CommandAllocator);
    }
}

void CommandContext::TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate) {
    D3D12_RESOURCE_STATES old_state = resource.m_UsageState;
    if (old_state != new_state) {
        assert(m_NumBarriersToFlush < 16 && "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& barrier_desc = m_Barriers[m_NumBarriersToFlush++];
        barrier_desc.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_desc.Transition.pResource    = resource.GetResource();
        barrier_desc.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier_desc.Transition.StateBefore  = old_state;
        barrier_desc.Transition.StateAfter   = new_state;

        if (new_state == resource.m_TransitioningState) {
            barrier_desc.Flags            = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            resource.m_TransitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
        } else {
            barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        }
        resource.m_UsageState = new_state;
    }

    if (flush_immediate || m_NumBarriersToFlush == 16) FlushResourceBarriers();
}

void CommandContext::FlushResourceBarriers() {
    if (m_NumBarriersToFlush > 0) {
        m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_Barriers.data());
        m_NumBarriersToFlush = 0;
    }
}

void CommandContext::BindDescriptorHeaps() {
    unsigned                                                                non_null_heaps = 0;
    std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> heaps_to_bind;
    for (unsigned i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        ID3D12DescriptorHeap* heap = m_CurrentDescriptorHeaps[i];
        if (heap != nullptr)
            heaps_to_bind[non_null_heaps++] = heap;
    }

    if (non_null_heaps > 0) m_CommandList->SetDescriptorHeaps(non_null_heaps, heaps_to_bind.data());
}

uint64_t CommandContext::Finish(bool wait_for_complete) {
    FlushResourceBarriers();

    CommandQueue& queue = m_Driver.GetCmdMgr().GetQueue(m_Type);

    uint64_t fence_value = queue.ExecuteCommandList(m_CommandList);
    m_CpuLinearAllocator.SetFence(fence_value);
    m_GpuLinearAllocator.SetFence(fence_value);
    queue.DiscardAllocator(fence_value, m_CommandAllocator);
    m_CommandAllocator = nullptr;

    m_DynamicViewDescriptorHeap.Reset(fence_value);
    m_DynamicSamplerDescriptorHeap.Reset(fence_value);
    m_CurrentDescriptorHeaps = {};

    if (wait_for_complete) m_Driver.GetCmdMgr().WaitForFence(fence_value);

    return fence_value;
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
    auto vp = CD3DX12_VIEWPORT(x, y, width, height);
    m_CommandList->RSSetViewports(1, &vp);
}

void GraphicsCommandContext::SetScissorRect(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) {
    auto rect = CD3DX12_RECT(left, top, right, bottom);
    m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsCommandContext::SetViewPortAndScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    SetViewPort(x, y, width, height);
    SetScissorRect(x, y, x + width, y + height);
}

void GraphicsCommandContext::SetRenderTarget(Graphics::RenderTarget& rt) {
    auto& render_target = *rt.GetBackend<RenderTarget>();
    TransitionResource(render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->OMSetRenderTargets(1, &render_target.GetRTV().handle, false, nullptr);
}

void GraphicsCommandContext::UnsetRenderTarget() {
    m_CommandList->OMSetRenderTargets(0, nullptr, false, nullptr);
}

void GraphicsCommandContext::SetRenderTargetAndDepthBuffer(Graphics::RenderTarget& rt, Graphics::DepthBuffer& depth_buffer) {
    auto& render_target = *rt.GetBackend<RenderTarget>();
    auto& db            = *depth_buffer.GetBackend<DepthBuffer>();
    TransitionResource(render_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->OMSetRenderTargets(1, &render_target.GetRTV().handle, false, &db.GetDSV().handle);
}

void GraphicsCommandContext::ClearRenderTarget(Graphics::RenderTarget& rt) {
    auto& render_target = *rt.GetBackend<RenderTarget>();
    TransitionResource(render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->ClearRenderTargetView(render_target.GetRTV().handle, vec4f(0, 0, 0, 1), 0, nullptr);
}

void GraphicsCommandContext::ClearDepthBuffer(Graphics::DepthBuffer& depth_buffer) {
    auto& db = *depth_buffer.GetBackend<DepthBuffer>();
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
    auto& render_target = *rt.GetBackend<RenderTarget>();
    TransitionResource(render_target, D3D12_RESOURCE_STATE_PRESENT, true);
}

void GraphicsCommandContext::Draw(const Graphics::MeshBuffer& mesh) {
    auto  index_buffer = mesh.indices->GetBackend<IndexBuffer>();
    auto& layout       = m_Pipeline->GetInputLayout();

    auto ibv = index_buffer->IndexBufferView();
    m_CommandList->IASetIndexBuffer(&ibv);
    for (auto&& [semanticName, vertex] : mesh.vertices) {
        auto iter = layout.begin();
        while (iter != layout.end() && iter->semantic_name != semanticName) iter++;
        if (iter == layout.end()) continue;

        auto vbv = vertex->GetBackend<VertexBuffer>()->VertexBufferView();
        m_CommandList->IASetVertexBuffers(iter->input_slot, 1, &vbv);
    }

    FlushResourceBarriers();
    m_DynamicViewDescriptorHeap.CommitStagedDescriptors(*this, &ID3D12GraphicsCommandList5::SetGraphicsRootDescriptorTable);
    m_DynamicSamplerDescriptorHeap.CommitStagedDescriptors(*this, &ID3D12GraphicsCommandList5::SetGraphicsRootDescriptorTable);
    m_CommandList->IASetPrimitiveTopology(to_dx_topology(mesh.primitive));
    m_CommandList->DrawIndexedInstanced(
        mesh.index_count.has_value() ? mesh.index_count.value() : index_buffer->GetElementCount(),
        1,
        mesh.index_offset.has_value() ? mesh.index_offset.value() : 0,
        mesh.vertex_offset.has_value() ? mesh.vertex_offset.value() : 0,
        0);
}

void CopyCommandContext::InitializeBuffer(GpuResource& dest, const uint8_t* data, size_t data_size) {
    auto upload_buffer = m_CpuLinearAllocator.Allocate(data_size);
    std::copy_n(data, data_size, upload_buffer.cpu_ptr);

    TransitionResource(dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    m_CommandList->CopyBufferRegion(dest.GetResource(), 0, upload_buffer.page_from.lock()->GetResource(),
                                    upload_buffer.page_offset, data_size);
    TransitionResource(dest, D3D12_RESOURCE_STATE_COMMON, true);

    Finish(true);
}

void CopyCommandContext::InitializeTexture(GpuResource& dest, const std::vector<D3D12_SUBRESOURCE_DATA>& sub_data) {
    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(dest.GetResource(), 0, sub_data.size());
    auto         upload_buffer      = m_CpuLinearAllocator.Allocate(upload_buffer_size);

    UpdateSubresources(m_CommandList, dest.GetResource(), upload_buffer.page_from.lock()->GetResource(),
                       upload_buffer.page_offset, 0, sub_data.size(), const_cast<D3D12_SUBRESOURCE_DATA*>(sub_data.data()));
    TransitionResource(dest, D3D12_RESOURCE_STATE_COMMON);

    Finish(true);
}

}  // namespace Hitagi::Graphics::backend::DX12