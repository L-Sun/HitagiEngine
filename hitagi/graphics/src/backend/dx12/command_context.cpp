#include "dx12_device.hpp"
#include "command_context.hpp"
#include "gpu_buffer.hpp"
#include "sampler.hpp"
#include "utils.hpp"

#include <hitagi/math/vector.hpp>

#include <magic_enum.hpp>

using namespace hitagi::math;

namespace hitagi::graphics::backend::DX12 {

CommandContext::CommandContext(DX12Device& device, D3D12_COMMAND_LIST_TYPE type)
    : m_Device(device),
      m_CpuLinearAllocator(device.GetDevice(), AllocationPageType::CPU_WRITABLE, [&device](uint64_t fence) { return device.GetCmdMgr().IsFenceComplete(fence); }),
      m_GpuLinearAllocator(device.GetDevice(), AllocationPageType::GPU_EXCLUSIVE, [&device](uint64_t fence) { return device.GetCmdMgr().IsFenceComplete(fence); }),
      m_ResourceBinder(device.GetDevice(), *this, [&device](uint64_t fence) { return device.GetCmdMgr().IsFenceComplete(fence); }),
      m_Type(type) {
    device.GetCmdMgr().CreateNewCommandList(m_Type, &m_CommandList, &m_CommandAllocator);
}
CommandContext::~CommandContext() {
    if (m_CommandList) m_CommandList->Release();
    // Finish() is not called before destruction. so use the last completed fence
    if (m_CommandAllocator) {
        auto&    queue       = m_Device.GetCmdMgr().GetQueue(m_Type);
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

    CommandQueue& queue = m_Device.GetCmdMgr().GetQueue(m_Type);

    uint64_t fence_value = queue.ExecuteCommandList(m_CommandList);
    m_CpuLinearAllocator.SetFence(fence_value);
    m_GpuLinearAllocator.SetFence(fence_value);
    queue.DiscardAllocator(fence_value, m_CommandAllocator);
    m_CommandAllocator = nullptr;

    m_ResourceBinder.Reset(fence_value);
    m_CurrentDescriptorHeaps = {};

    if (wait_for_complete) m_Device.GetCmdMgr().WaitForFence(fence_value);

    return fence_value;
}

void CommandContext::Reset() {
    // We only call Reset() on previously freed contexts.  The command list persists, but we must
    // request a new allocator.
    assert(m_CommandList != nullptr && m_CommandAllocator == nullptr);
    m_CommandAllocator = m_Device.GetCmdMgr().GetQueue(m_Type).RequestAllocator();
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

void GraphicsCommandContext::SetRenderTarget(std::shared_ptr<graphics::RenderTarget> render_target) {
    auto rt = render_target->GetBackend<RenderTarget>();
    TransitionResource(*rt, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->OMSetRenderTargets(1,
                                      &(rt->GetRTV()->handle),
                                      false,
                                      nullptr);
}

void GraphicsCommandContext::UnsetRenderTarget() {
    m_CommandList->OMSetRenderTargets(0, nullptr, false, nullptr);
}

void GraphicsCommandContext::SetRenderTargetAndDepthBuffer(std::shared_ptr<graphics::RenderTarget> render_target, std::shared_ptr<graphics::DepthBuffer> depth_buffer) {
    assert(render_target != nullptr && depth_buffer != nullptr && "Can set a null render target or depth buffer!");

    auto rt = render_target->GetBackend<RenderTarget>();
    auto db = depth_buffer->GetBackend<DepthBuffer>();

    assert(rt != nullptr && fmt::format("the render target does not contain a gpu resource! rt:{}", render_target->GetName()).c_str());
    assert(db != nullptr && fmt::format("the depth buffer does not contain a gpu resource! db:{}", render_target->GetName()).c_str());

    TransitionResource(*rt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(*db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->OMSetRenderTargets(1,
                                      &(rt->GetRTV()->handle),
                                      false,
                                      &(db->GetDSV()->handle));
}

void GraphicsCommandContext::ClearRenderTarget(std::shared_ptr<graphics::RenderTarget> render_target) {
    auto rt = render_target->GetBackend<RenderTarget>();
    TransitionResource(*rt, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->ClearRenderTargetView(rt->GetRTV()->handle,
                                         vec4f(0, 0, 0, 1),
                                         0,
                                         nullptr);
}

void GraphicsCommandContext::ClearDepthBuffer(std::shared_ptr<graphics::DepthBuffer> depth_buffer) {
    auto db = depth_buffer->GetBackend<DepthBuffer>();
    TransitionResource(*db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->ClearDepthStencilView(
        db->GetDSV()->handle,
        D3D12_CLEAR_FLAG_DEPTH,
        db->GetClearDepth(),
        db->GetClearStencil(),
        0,
        nullptr);
}

void GraphicsCommandContext::SetPipelineState(std::shared_ptr<graphics::PipelineState> pipeline) {
    assert(pipeline != nullptr && pipeline->GetBackend<PSO>() != nullptr);
    if (m_CurrentPipeline == pipeline->GetBackend<PSO>()) return;

    m_CurrentPipeline = pipeline->GetBackend<PSO>();
    m_CommandList->SetPipelineState(m_CurrentPipeline->GetPSO());

    if (m_CurrRootSignature != m_CurrentPipeline->GetRootSignature().Get()) {
        m_CurrRootSignature = m_CurrentPipeline->GetRootSignature().Get();
        m_CommandList->SetGraphicsRootSignature(m_CurrRootSignature);
        m_ResourceBinder.ParseRootSignature(m_CurrentPipeline->GetRootSignatureDesc());
    }
}

void GraphicsCommandContext::BindResource(std::uint32_t slot, std::shared_ptr<graphics::ConstantBuffer> cb, size_t offset) {
    assert(cb != nullptr && cb->GetBackend<ConstantBuffer>() != nullptr);

    if (m_CurrentPipeline == nullptr) {
        throw std::logic_error("pipeline must be set before setting parameters");
    }
    m_ResourceBinder.BindResource(slot, cb->GetBackend<ConstantBuffer>(), offset);
}

void GraphicsCommandContext::BindResource(std::uint32_t slot, std::shared_ptr<graphics::TextureBuffer> texture) {
    assert(texture != nullptr && texture->GetBackend<TextureBuffer>() != nullptr);

    if (m_CurrentPipeline == nullptr) {
        throw std::logic_error("pipeline must be set before setting parameters");
    }
    m_ResourceBinder.BindResource(slot, texture->GetBackend<TextureBuffer>());
}

void GraphicsCommandContext::BindResource(std::uint32_t slot, std::shared_ptr<graphics::Sampler> sampler) {
    assert(sampler != nullptr && sampler->GetBackend<Sampler>() != nullptr);

    if (m_CurrentPipeline == nullptr) {
        throw std::logic_error("pipeline must be set before setting parameters");
    }
    m_ResourceBinder.BindResource(slot, sampler->GetBackend<Sampler>());
}

void GraphicsCommandContext::Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count) {
    if (data == nullptr || count == 0) return;
    if (m_CurrentPipeline == nullptr) {
        throw std::logic_error("pipeline must be set before setting parameters");
    }
    m_ResourceBinder.Set32BitsConstants(slot, data, count);
}

void GraphicsCommandContext::Present(std::shared_ptr<graphics::RenderTarget> render_target) {
    assert(render_target != nullptr && render_target->GetBackend<RenderTarget>() != nullptr);

    auto rt = render_target->GetBackend<RenderTarget>();
    TransitionResource(*rt, D3D12_RESOURCE_STATE_PRESENT, true);
}

void GraphicsCommandContext::Draw(
    std::shared_ptr<graphics::VertexBuffer> vertex_buffer,
    std::shared_ptr<graphics::IndexBuffer>  index_buffer,
    resource::PrimitiveType                 primitive,
    std::size_t                             index_count,
    std::size_t                             vertex_offset,
    std::size_t                             index_offset) {
    assert(vertex_buffer != nullptr && index_buffer != nullptr);
    assert(vertex_offset < vertex_buffer->desc.vertex_count);
    assert(index_offset + index_count <= index_buffer->desc.index_count);

    auto vb = vertex_buffer->GetBackend<VertexBuffer>();
    auto ib = index_buffer->GetBackend<IndexBuffer>();

    assert(vb != nullptr && ib != nullptr);

    auto ibv = ib->IndexBufferView();
    m_CommandList->IASetIndexBuffer(&ibv);

    auto gpso = static_cast<GraphicsPSO*>(m_CurrentPipeline);

    magic_enum::enum_for_each<resource::VertexAttribute>([&](auto attr) {
        if (vb->AttributeEnabled(attr())) {
            if (int slot = gpso->GetAttributeSlot(attr()); slot != -1) {
                auto vbv = vertex_buffer->GetBackend<VertexBuffer>()->VertexBufferView(attr());
                m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
            }
        }
    });

    FlushResourceBarriers();
    m_ResourceBinder.CommitStagedDescriptors();
    m_CommandList->IASetPrimitiveTopology(to_dx_topology(primitive));
    m_CommandList->DrawIndexedInstanced(
        index_count,
        1,
        index_offset,
        vertex_offset,
        0);
}

void GraphicsCommandContext::UpdateBuffer(std::shared_ptr<graphics::Resource> resource, std::size_t offset, const std::byte* data, std::size_t data_size) {
    auto dest = resource->GetBackend<GpuBuffer>();
    assert(dest->GetBufferSize() - offset >= data_size);

    auto upload_buffer = m_CpuLinearAllocator.Allocate(data_size);
    std::copy_n(data, data_size, upload_buffer.cpu_ptr);

    auto old_state = dest->m_UsageState;
    TransitionResource(*dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    m_CommandList->CopyBufferRegion(dest->GetResource(),
                                    offset,
                                    upload_buffer.page_from.lock()->GetResource(),
                                    upload_buffer.page_offset,
                                    data_size);
    TransitionResource(*dest, old_state, true);
}

void ComputeCommandContext::SetPipelineState(const std::shared_ptr<graphics::PipelineState>& pipeline) {
    assert(pipeline != nullptr && pipeline->GetBackend<PSO>() != nullptr);
    if (m_CurrentPipeline == pipeline->GetBackend<PSO>()) return;

    m_CurrentPipeline = pipeline->GetBackend<PSO>();
    m_CommandList->SetPipelineState(m_CurrentPipeline->GetPSO());

    if (m_CurrRootSignature != m_CurrentPipeline->GetRootSignature().Get()) {
        m_CurrRootSignature = m_CurrentPipeline->GetRootSignature().Get();
        m_CommandList->SetComputeRootSignature(m_CurrRootSignature);
    }
}

void CopyCommandContext::InitializeBuffer(GpuResource& dest, const std::byte* data, size_t data_size) {
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

}  // namespace hitagi::graphics::backend::DX12