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
    DX12CommandContext(DX12Device& device, CommandType type, std::string_view name, std::uint64_t& fence_value);
    template <typename T>
    void TransitionResource(T& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate = false);
    void FlushBarriers();

protected:
    friend class DX12CommandQueue;
    friend class ResourceBinder;

    DX12Device&                              m_Device;
    CommandType                              m_Type;
    ComPtr<ID3D12CommandAllocator>           m_CmdAllocator;
    ComPtr<ID3D12GraphicsCommandList5>       m_CmdList;
    std::pmr::vector<D3D12_RESOURCE_BARRIER> m_Barriers;

    ID3D12PipelineState* m_CurrentPSO = nullptr;
    ResourceBinder       m_ResourceBinder;

    std::uint64_t& m_FenceValue;
};

class DX12GraphicsCommandContext : public GraphicsCommandContext, public DX12CommandContext {
public:
    DX12GraphicsCommandContext(DX12Device& device, std::string_view name);
    void SetName(std::string_view name) final;
    void ResetState(GpuBuffer& buffer) final;
    void ResetState(Texture& texture) final;

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::vec4f& color) final;
    void SetRenderTarget(Texture& target) final;
    void SetRenderTargetAndDepthStencil(Texture& target, Texture& depth_stencil) final;

    void ClearRenderTarget(Texture& target) final;
    void ClearDepthStencil(Texture& depth_stencil) final;

    void SetPipeline(const RenderPipeline& pipeline) final;
    void SetIndexBuffer(GpuBuffer& buffer) final;
    void SetVertexBuffer(std::uint8_t slot, GpuBuffer& buffer) final;

    void PushConstant(std::uint32_t slot, const std::span<const std::byte>& data) final;
    void BindConstantBuffer(std::uint32_t slot, GpuBuffer& buffer, std::size_t index = 0) final;
    void BindTexture(std::uint32_t slot, Texture& texture) final;
    int  GetBindless(const Texture& texture) final;

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final;

    void CopyTexture(const Texture& src, Texture& dest) final;
    void Present(Texture& back_buffer) final;

    void Reset() final;
    void End() final;
};

class DX12ComputeCommandContext : public ComputeCommandContext, public DX12CommandContext {
public:
    DX12ComputeCommandContext(DX12Device& device, std::string_view name);
    void SetName(std::string_view name) final;
    void ResetState(GpuBuffer& buffer) final;
    void ResetState(Texture& texture) final;

    void Reset() final;
    void End() final;
};

class DX12CopyCommandContext : public CopyCommandContext, public DX12CommandContext {
public:
    DX12CopyCommandContext(DX12Device& device, std::string_view name);
    void SetName(std::string_view name) final;
    void ResetState(GpuBuffer& buffer) final;
    void ResetState(Texture& texture) final;

    void CopyBuffer(const GpuBuffer& src, std::size_t src_offset, GpuBuffer& dest, std::size_t dest_offset, std::size_t size) final;
    void CopyTexture(const Texture& src, const Texture& dest) final;

    void Reset() final;
    void End() final;

    ID3D12GraphicsCommandList5* GetCmdList() const noexcept { return m_CmdList.Get(); }
};

template <typename T>
void DX12CommandContext::TransitionResource(T& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate) {
    auto& d3d_res = static_cast<DX12ResourceWrapper<T>&>(resource);
    if (d3d_res.state != new_state) {
        m_Barriers.emplace_back(D3D12_RESOURCE_BARRIER{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            // TODO use split barrier for more paralle performence
            .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource   = d3d_res.resource.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = d3d_res.state,
                .StateAfter  = new_state,
            },
        });

        d3d_res.state = new_state;
    }
    if (flush_immediate || m_Barriers.size() >= 16) FlushBarriers();
}

}  // namespace hitagi::gfx