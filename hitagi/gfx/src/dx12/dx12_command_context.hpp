#pragma once
#include "dx12_resource.hpp"
#include "resource_binder.hpp"
#include <hitagi/gfx/command_context.hpp>

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {

class DX12Device;

class DX12CommandContext {
public:
    DX12CommandContext(DX12Device* device, CommandType type, std::string_view name);

    template <typename T>
    void TransitionResource(DX12ResourceWrapper<T>& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate = false);
    void FlushBarriers();

protected:
    friend class DX12CommandQueue;
    friend class ResourceBinder;

    DX12Device*                              m_Device;
    CommandType                              m_Type;
    ComPtr<ID3D12CommandAllocator>           m_CmdAllocator;
    ComPtr<ID3D12GraphicsCommandList5>       m_CmdList;
    std::pmr::vector<D3D12_RESOURCE_BARRIER> m_Barriers;

    ID3D12PipelineState* m_CurrentPSO = nullptr;
    ResourceBinder       m_ResourceBinder;
};

class DX12GraphicsCommandContext : public GraphicsCommandContext, public DX12CommandContext {
public:
    DX12GraphicsCommandContext(DX12Device* device, std::string_view name);

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::vec4f& color) final;
    void SetRenderTarget(const TextureView& target) final;
    void SetRenderTargetAndDepthStencil(const TextureView& target, const TextureView& depth_stencil) final;

    void ClearRenderTarget(const TextureView& target) final;
    void ClearDepthStencil(const TextureView& depth_stencil) final;

    void SetPipeline(const RenderPipeline& pipeline) final;
    void SetIndexBuffer(const GpuBufferView& buffer) final;
    void SetVertexBuffer(std::uint8_t slot, const GpuBufferView& buffer) final;

    void PushConstant(std::uint32_t slot, const std::span<const std::byte>& data) final;
    void BindConstantBuffer(std::uint32_t slot, const GpuBufferView& buffer, std::size_t index = 0) final;
    int  GetBindless(const TextureView& texture_view) final;

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final;

    void Present(const TextureView& render_target) final;

    void End() final;
};

class DX12ComputeCommandContext : public ComputeCommandContext, public DX12CommandContext {
public:
    DX12ComputeCommandContext(DX12Device* device, std::string_view name);

    void End() final;
};

class DX12CopyCommandContext : public CopyCommandContext, public DX12CommandContext {
public:
    DX12CopyCommandContext(DX12Device* device, std::string_view name);

    void CopyBuffer(const GpuBuffer& src, std::size_t src_offset, GpuBuffer& dest, std::size_t dest_offset, std::size_t size) final;
    void End() final;

    ID3D12GraphicsCommandList5* GetCmdList() const noexcept { return m_CmdList.Get(); }
};

template <typename T>
void DX12CommandContext::TransitionResource(DX12ResourceWrapper<T>& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate) {
    if (resource.state != new_state) {
        m_Barriers.emplace_back(D3D12_RESOURCE_BARRIER{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            // TODO use split barrier for more paralle performence
            .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource   = resource.resource.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = resource.state,
                .StateAfter  = new_state,
            },
        });

        resource.state = new_state;
    }
    if (flush_immediate || m_Barriers.size() >= 16) FlushBarriers();
}

}  // namespace hitagi::gfx