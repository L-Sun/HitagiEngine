#pragma once

#include "gpu_resource.hpp"
#include "allocator.hpp"
#include "resource_binder.hpp"
#include "pso.hpp"

#include <hitagi/graphics/command_context.hpp>

namespace hitagi::graphics::backend::DX12 {
class DX12Device;

class CommandContext {
public:
    CommandContext(DX12Device* device, D3D12_COMMAND_LIST_TYPE type);
    CommandContext(const CommandContext&)            = delete;
    CommandContext& operator=(const CommandContext&) = delete;
    CommandContext(CommandContext&&)                 = default;
    CommandContext& operator=(CommandContext&&)      = delete;
    ~CommandContext();

    auto GetType() const noexcept { return m_Type; }
    auto GetCommandList() noexcept { return m_CommandList; }

    void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate = false);
    void FlushResourceBarriers();

    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap) {
        if (m_CurrentDescriptorHeaps[type] != heap) {
            m_CurrentDescriptorHeaps[type] = heap;
            BindDescriptorHeaps();
        }
    }

    uint64_t Finish(bool wait_for_complete = false);
    void     Reset();

protected:
    void BindDescriptorHeaps();

    DX12Device*                 m_Device;
    ID3D12GraphicsCommandList5* m_CommandList      = nullptr;
    ID3D12CommandAllocator*     m_CommandAllocator = nullptr;

    std::array<D3D12_RESOURCE_BARRIER, 16> m_Barriers{};
    unsigned                               m_NumBarriersToFlush = 0;

    LinearAllocator m_CpuLinearAllocator;
    LinearAllocator m_GpuLinearAllocator;

    std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_CurrentDescriptorHeaps{};

    ResourceBinder m_ResourceBinder;

    ID3D12RootSignature* m_CurrRootSignature = nullptr;
    PSO*                 m_CurrentPipeline   = nullptr;

    D3D12_COMMAND_LIST_TYPE m_Type;
};

class GraphicsCommandContext : public CommandContext, public graphics::IGraphicsCommandContext {
public:
    GraphicsCommandContext(DX12Device* device) : CommandContext(device, D3D12_COMMAND_LIST_TYPE_DIRECT) {}

    // Front end interface
    void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) final;
    void SetScissorRect(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) final;
    void SetViewPortAndScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) final;
    void SetRenderTarget(const graphics::RenderTarget& rt) final;
    void UnsetRenderTarget() final;
    void SetRenderTargetAndDepthBuffer(const graphics::RenderTarget& rt, const graphics::DepthBuffer& depth_buffer) final;
    void ClearRenderTarget(const graphics::RenderTarget& rt) final;
    void ClearDepthBuffer(const graphics::DepthBuffer& depth_buffer) final;
    void SetPipelineState(const graphics::PipelineState& pipeline) final;
    void BindResource(std::uint32_t slot, const graphics::ConstantBuffer& constant_buffer, std::size_t offset) final;
    void BindResource(std::uint32_t slot, const resource::Texture& texture) final;
    void BindResource(std::uint32_t slot, const graphics::Sampler& sampler) final;
    void Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count) final;

    void UpdateBuffer(Resource* resource, std::size_t offset, const std::byte* data, std::size_t data_size) final;
    void UpdateVertexBuffer(resource::VertexArray& vertices) final;
    void UpdateIndexBuffer(resource::IndexArray& indices) final;

    void          Draw(const resource::VertexArray& vb,
                       const resource::IndexArray&  ib,
                       resource::PrimitiveType,
                       std::size_t index_count,
                       std::size_t vertex_offset,
                       std::size_t index_offset) final;
    void          Present(const graphics::RenderTarget& rt) final;
    std::uint64_t Finish(bool wait_for_complete = false) final { return CommandContext::Finish(wait_for_complete); }
};

class ComputeCommandContext : public CommandContext {
public:
    ComputeCommandContext(DX12Device* device) : CommandContext(device, D3D12_COMMAND_LIST_TYPE_COMPUTE) {}

    void SetPipelineState(const std::shared_ptr<graphics::PipelineState>& pipeline);
};

class CopyCommandContext : public CommandContext {
public:
    CopyCommandContext(DX12Device* device) : CommandContext(device, D3D12_COMMAND_LIST_TYPE_COPY) {}

    void InitializeBuffer(GpuBuffer& dest, const std::byte* data, size_t data_size);
    void InitializeTexture(resource::Texture& texture, const std::vector<D3D12_SUBRESOURCE_DATA>& sub_data);
};

}  // namespace hitagi::graphics::backend::DX12