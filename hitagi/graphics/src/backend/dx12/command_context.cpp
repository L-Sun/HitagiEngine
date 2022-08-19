#include "dx12_device.hpp"
#include "command_context.hpp"
#include "gpu_buffer.hpp"
#include "utils.hpp"

#include <hitagi/math/vector.hpp>

#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::graphics::backend::DX12 {

CommandContext::CommandContext(std::string_view name, DX12Device* device, D3D12_COMMAND_LIST_TYPE type)
    : m_Device(device),
      m_CpuLinearAllocator(device, AllocationPageType::CPU_WRITABLE, [device](uint64_t fence) { return device->GetCmdMgr().IsFenceComplete(fence); }),
      m_GpuLinearAllocator(device, AllocationPageType::GPU_EXCLUSIVE, [device](uint64_t fence) { return device->GetCmdMgr().IsFenceComplete(fence); }),
      m_ResourceBinder(device, *this, [device](uint64_t fence) { return device->GetCmdMgr().IsFenceComplete(fence); }),
      m_Type(type) {
    device->GetCmdMgr().CreateNewCommandList(name, m_Type, &m_CommandList, &m_CommandAllocator);
}
CommandContext::~CommandContext() {
    if (m_CommandList) m_CommandList->Release();
    // Finish() is not called before destruction. so use the last completed fence
    if (m_CommandAllocator) {
        auto&    queue       = m_Device->GetCmdMgr().GetQueue(m_Type);
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

    CommandQueue& queue = m_Device->GetCmdMgr().GetQueue(m_Type);

    uint64_t fence_value = queue.ExecuteCommandList(m_CommandList);
    m_CpuLinearAllocator.SetFence(fence_value);
    m_GpuLinearAllocator.SetFence(fence_value);
    queue.DiscardAllocator(fence_value, m_CommandAllocator);
    m_CommandAllocator = nullptr;

    m_ResourceBinder.Reset(fence_value);
    m_CurrentDescriptorHeaps = {};

    if (wait_for_complete) m_Device->GetCmdMgr().WaitForFence(fence_value);

    return fence_value;
}

void CommandContext::Reset() {
    // We only call Reset() on previously freed contexts.  The command list persists, but we must
    // request a new allocator.
    assert(m_CommandList != nullptr && m_CommandAllocator == nullptr);
    m_CommandAllocator = m_Device->GetCmdMgr().GetQueue(m_Type).RequestAllocator();
    m_CommandList->Reset(m_CommandAllocator, nullptr);

    m_NumBarriersToFlush = 0;
}

void GraphicsCommandContext::ClearRenderTarget(const graphics::RenderTarget& render_target) {
    if (render_target.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can clear an uninitialized render target ({})!", render_target.name);
        return;
    }
    auto rt = render_target.gpu_resource->GetBackend<RenderTarget>();

    TransitionResource(*rt, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->ClearRenderTargetView(rt->GetRTV().handle,
                                         vec4f(0, 0, 0, 1),
                                         0,
                                         nullptr);
}

void GraphicsCommandContext::ClearDepthBuffer(const graphics::DepthBuffer& depth_buffer) {
    if (depth_buffer.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can clear an uninitialized depth buffer ({})!", depth_buffer.name);
        return;
    }
    auto db = depth_buffer.gpu_resource->GetBackend<DepthBuffer>();
    TransitionResource(*db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->ClearDepthStencilView(
        db->GetDSV().handle,
        D3D12_CLEAR_FLAG_DEPTH,
        db->GetClearDepth(),
        db->GetClearStencil(),
        0,
        nullptr);
}

void GraphicsCommandContext::SetRenderTarget(const graphics::RenderTarget& render_target) {
    if (render_target.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can not set an uninitialized render target ({})!", render_target.name);
        return;
    }
    auto rt = render_target.gpu_resource->GetBackend<RenderTarget>();
    TransitionResource(*rt, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    m_CommandList->OMSetRenderTargets(1,
                                      &(rt->GetRTV().handle),
                                      false,
                                      nullptr);
}

void GraphicsCommandContext::SetRenderTargetAndDepthBuffer(const graphics::RenderTarget& render_target, const graphics::DepthBuffer& depth_buffer) {
    if (render_target.gpu_resource == nullptr || depth_buffer.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can not set an uninitialized render target ({}) or depth buffer ({})!", render_target.name, depth_buffer.name);
        return;
    }

    auto rt = render_target.gpu_resource->GetBackend<RenderTarget>();
    auto db = depth_buffer.gpu_resource->GetBackend<DepthBuffer>();

    TransitionResource(*rt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(*db, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    m_CommandList->OMSetRenderTargets(1,
                                      &(rt->GetRTV().handle),
                                      false,
                                      &(db->GetDSV().handle));
}

void GraphicsCommandContext::SetPipelineState(const graphics::PipelineState& pipeline) {
    if (pipeline.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can Set an uninitialized pipeline ({})!", pipeline.name);
        return;
    }
    auto pso = pipeline.gpu_resource->GetBackend<PSO>();
    if (m_CurrentPipeline == pso) return;

    m_CurrentPipeline = pso;
    m_CommandList->SetPipelineState(m_CurrentPipeline->GetPSO());
    m_CommandList->IASetPrimitiveTopology(to_dx_topology(pipeline.primitive_type));

    if (m_CurrRootSignature != m_CurrentPipeline->GetRootSignature().Get()) {
        m_CurrRootSignature = m_CurrentPipeline->GetRootSignature().Get();
        m_CommandList->SetGraphicsRootSignature(m_CurrRootSignature);
        m_ResourceBinder.ParseRootSignature(m_CurrentPipeline->GetRootSignatureDesc());
    }
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

void GraphicsCommandContext::SetBlendFactor(math::vec4f color) {
    m_CommandList->OMSetBlendFactor(color);
}

void GraphicsCommandContext::BindResource(std::uint32_t slot, const graphics::ConstantBuffer& constant_buffer, size_t offset) {
    if (constant_buffer.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can Set an uninitialized constant buffer ({})!", constant_buffer.name);
        return;
    }

    if (m_CurrentPipeline == nullptr) {
        spdlog::get("GraphicsManager")->error("pipeline must be set before setting parameters!");
        return;
    }

    m_ResourceBinder.BindResource(slot, constant_buffer.gpu_resource->GetBackend<ConstantBuffer>(), offset);
}

void GraphicsCommandContext::BindResource(std::uint32_t slot, const resource::Texture& texture) {
    if (texture.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can Set an uninitialized texture ({})!", texture.name);
        return;
    }

    if (m_CurrentPipeline == nullptr) {
        spdlog::get("GraphicsManager")->error("pipeline must be set before setting parameters!");
        return;
    }

    m_ResourceBinder.BindResource(slot, texture.gpu_resource->GetBackend<Texture>());
}

void GraphicsCommandContext::BindResource(std::uint32_t slot, const graphics::Sampler& sampler) {
    if (sampler.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can Set an uninitialized sampler ({})!", sampler.name);
        return;
    }

    if (m_CurrentPipeline == nullptr) {
        spdlog::get("GraphicsManager")->error("pipeline must be set before setting parameters!");
        return;
    }

    m_ResourceBinder.BindResource(slot, sampler.gpu_resource->GetBackend<Sampler>());
}

void GraphicsCommandContext::Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count) {
    if (data == nullptr || count == 0) return;

    if (m_CurrentPipeline == nullptr) {
        spdlog::get("GraphicsManager")->error("pipeline must be set before setting parameters!");
        return;
    }

    m_ResourceBinder.Set32BitsConstants(slot, data, count);
}

void GraphicsCommandContext::BindDynamicStructuredBuffer(std::uint32_t slot, const std::byte* data, std::size_t size) {
    Allocation cb = m_CpuLinearAllocator.Allocate(size);
    std::copy_n(data, size, cb.cpu_ptr);
    m_ResourceBinder.BindDynamicStructuredBuffer(slot, cb.gpu_addr);
}

void GraphicsCommandContext::BindDynamicConstantBuffer(std::uint32_t slot, const std::byte* data, std::size_t size) {
    Allocation cb = m_CpuLinearAllocator.Allocate(size);
    std::copy_n(data, size, cb.cpu_ptr);
    m_ResourceBinder.BindDynamicConstantBuffer(slot, cb.gpu_addr);
}

void GraphicsCommandContext::BindVertexBuffer(const resource::VertexArray& vertices) {
    auto vb = vertices.gpu_resource->GetBackend<GpuBuffer>();
    if (vb == nullptr) {
        spdlog::get("GraphicsManager")->warn("Can not bind an uninitialized vertex buffer: ({})!", vertices.name);
        return;
    }
    auto        gpso      = static_cast<GraphicsPSO*>(m_CurrentPipeline);
    std::size_t vb_offset = 0;
    magic_enum::enum_for_each<resource::VertexAttribute>([&](auto attr) {
        if (vertices.IsEnabled(attr())) {
            D3D12_VERTEX_BUFFER_VIEW vbv{
                .BufferLocation = vb->GetResource()->GetGPUVirtualAddress() + vb_offset,
                .SizeInBytes    = static_cast<UINT>(get_vertex_attribute_size(attr()) * vertices.vertex_count),
                .StrideInBytes  = get_vertex_attribute_size(attr()),
            };
            vb_offset += vbv.SizeInBytes;
            if (int slot = gpso->GetAttributeSlot(attr()); slot != -1) {
                m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
            }
        }
    });
}

void GraphicsCommandContext::BindIndexBuffer(const resource::IndexArray& indices) {
    auto ib = indices.gpu_resource->GetBackend<GpuBuffer>();
    if (ib == nullptr) {
        spdlog::get("GraphicsManager")->warn("Can not bind an uninitialized index buffer: ({})!", indices.name);
        return;
    }
    D3D12_INDEX_BUFFER_VIEW ibv{
        .BufferLocation = ib->GetResource()->GetGPUVirtualAddress(),
        .SizeInBytes    = static_cast<UINT>(ib->GetBufferSize()),
        .Format         = index_type_to_dxgi_format(indices.type),
    };
    m_CommandList->IASetIndexBuffer(&ibv);
}

void GraphicsCommandContext::BindDynamicVertexBuffer(const resource::VertexArray& vertices) {
    if (vertices.cpu_buffer.Empty()) {
        spdlog::get("GraphicsManager")->warn("Can not bind an empty vertex buffer: ({})!", vertices.name);
        return;
    }
    Allocation vb = m_CpuLinearAllocator.Allocate(vertices.cpu_buffer.GetDataSize());
    std::copy_n(vertices.cpu_buffer.GetData(), vertices.cpu_buffer.GetDataSize(), vb.cpu_ptr);

    auto        gpso      = static_cast<GraphicsPSO*>(m_CurrentPipeline);
    std::size_t vb_offset = 0;
    magic_enum::enum_for_each<resource::VertexAttribute>([&](auto attr) {
        if (vertices.IsEnabled(attr())) {
            D3D12_VERTEX_BUFFER_VIEW vbv{
                .BufferLocation = vb.gpu_addr + vb_offset,
                .SizeInBytes    = static_cast<UINT>(get_vertex_attribute_size(attr()) * vertices.vertex_count),
                .StrideInBytes  = get_vertex_attribute_size(attr()),
            };
            vb_offset += vbv.SizeInBytes;
            if (int slot = gpso->GetAttributeSlot(attr()); slot != -1) {
                m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
            }
        }
    });
}

void GraphicsCommandContext::BindDynamicIndexBuffer(const resource::IndexArray& indices) {
    if (indices.cpu_buffer.Empty()) {
        spdlog::get("GraphicsManager")->warn("Can not bind an empty index buffer: ({})!", indices.name);
        return;
    }
    Allocation ib = m_CpuLinearAllocator.Allocate(indices.cpu_buffer.GetDataSize());
    std::copy_n(indices.cpu_buffer.GetData(), indices.cpu_buffer.GetDataSize(), ib.cpu_ptr);

    D3D12_INDEX_BUFFER_VIEW ibv{
        .BufferLocation = ib.gpu_addr,
        .SizeInBytes    = static_cast<UINT>(ib.size),
        .Format         = index_type_to_dxgi_format(indices.type),
    };
    m_CommandList->IASetIndexBuffer(&ibv);
}

void GraphicsCommandContext::Draw(std::size_t vertex_count, std::size_t vertex_start_offset) {
    DrawInstanced(vertex_count, 1, vertex_start_offset, 0);
}

void GraphicsCommandContext::DrawIndexed(std::size_t index_count, std::size_t start_index_location, std::size_t base_vertex_location) {
    DrawIndexedInstanced(index_count, 1, start_index_location, base_vertex_location, 0);
}

void GraphicsCommandContext::DrawInstanced(std::size_t vertex_count, std::size_t instance_count, std::size_t start_vertex_location, std::size_t start_instance_location) {
    FlushResourceBarriers();
    m_ResourceBinder.CommitStagedDescriptors();
    m_CommandList->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
}

void GraphicsCommandContext::DrawIndexedInstanced(std::size_t index_count, std::size_t instance_count, std::size_t start_index_location, std::size_t base_vertex_location, std::size_t start_instance_location) {
    FlushResourceBarriers();
    m_ResourceBinder.CommitStagedDescriptors();
    m_CommandList->DrawIndexedInstanced(index_count, instance_count, start_index_location, base_vertex_location, start_instance_location);
}

void GraphicsCommandContext::Present(const graphics::RenderTarget& render_target) {
    if (render_target.gpu_resource == nullptr) {
        spdlog::get("GraphicsManager")->error("Can clear an uninitialized render target ({})!", render_target.name);
        return;
    }
    auto rt = render_target.gpu_resource->GetBackend<RenderTarget>();
    TransitionResource(*rt, D3D12_RESOURCE_STATE_PRESENT, true);
}

void GraphicsCommandContext::UpdateBuffer(Resource* resource, std::size_t offset, const std::byte* data, std::size_t data_size) {
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

void GraphicsCommandContext::UpdateVertexBuffer(resource::VertexArray& vertices) {
    if (!vertices.dirty) return;

    if (vertices.gpu_resource == nullptr || vertices.cpu_buffer.GetDataSize() > vertices.gpu_resource->GetBackend<GpuBuffer>()->GetBufferSize()) {
        m_Device->InitVertexBuffer(vertices);
    }

    UpdateBuffer(vertices.gpu_resource.get(), 0, vertices.cpu_buffer.GetData(), vertices.cpu_buffer.GetDataSize());
    vertices.dirty = false;
}

void GraphicsCommandContext::UpdateIndexBuffer(resource::IndexArray& indices) {
    if (!indices.dirty) return;

    if (indices.gpu_resource == nullptr || indices.cpu_buffer.GetDataSize() > indices.gpu_resource->GetBackend<GpuBuffer>()->GetBufferSize())
        m_Device->InitIndexBuffer(indices);

    UpdateBuffer(indices.gpu_resource.get(), 0, indices.cpu_buffer.GetData(), indices.cpu_buffer.GetDataSize());
    indices.dirty = false;
}

void ComputeCommandContext::SetPipelineState(const std::shared_ptr<graphics::PipelineState>& pipeline) {
    assert(pipeline != nullptr && pipeline->gpu_resource != nullptr);
    if (m_CurrentPipeline == pipeline->gpu_resource->GetBackend<PSO>()) return;

    m_CurrentPipeline = pipeline->gpu_resource->GetBackend<PSO>();
    m_CommandList->SetPipelineState(m_CurrentPipeline->GetPSO());
    m_CommandList->IASetPrimitiveTopology(to_dx_topology(pipeline->primitive_type));

    if (m_CurrRootSignature != m_CurrentPipeline->GetRootSignature().Get()) {
        m_CurrRootSignature = m_CurrentPipeline->GetRootSignature().Get();
        m_CommandList->SetComputeRootSignature(m_CurrRootSignature);
        m_ResourceBinder.ParseRootSignature(m_CurrentPipeline->GetRootSignatureDesc());
    }
}

void CopyCommandContext::InitializeBuffer(GpuBuffer& dest, const std::byte* data, size_t data_size) {
    auto upload_buffer = m_CpuLinearAllocator.Allocate(data_size);
    std::copy_n(data, data_size, upload_buffer.cpu_ptr);

    TransitionResource(dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    m_CommandList->CopyBufferRegion(dest.GetResource(), 0, upload_buffer.page_from.lock()->GetResource(),
                                    upload_buffer.page_offset, data_size);
    TransitionResource(dest, D3D12_RESOURCE_STATE_COMMON, true);

    Finish(true);
}

void CopyCommandContext::InitializeTexture(resource::Texture& texture, const std::pmr::vector<D3D12_SUBRESOURCE_DATA>& sub_data) {
    auto dest = texture.gpu_resource->GetBackend<Texture>();

    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(dest->GetResource(), 0, sub_data.size());
    auto         upload_buffer      = m_CpuLinearAllocator.Allocate(upload_buffer_size);

    UpdateSubresources(m_CommandList, dest->GetResource(), upload_buffer.page_from.lock()->GetResource(),
                       upload_buffer.page_offset, 0, sub_data.size(), const_cast<D3D12_SUBRESOURCE_DATA*>(sub_data.data()));
    TransitionResource(*dest, D3D12_RESOURCE_STATE_COMMON);

    Finish(true);
}

}  // namespace hitagi::graphics::backend::DX12