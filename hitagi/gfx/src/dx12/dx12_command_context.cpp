#include "dx12_command_context.hpp"
#include "dx12_device.hpp"
#include "dx12_resource.hpp"
#include "utils.hpp"

namespace hitagi::gfx {
template <typename T>
auto initialize_command_context(DX12Device& device, CommandType type, std::string_view name, ComPtr<ID3D12CommandAllocator>& cmd_allocator, ComPtr<T>& cmd_list) {
    ThrowIfFailed(device.GetDevice()->CreateCommandAllocator(
        to_d3d_command_type(type),
        IID_PPV_ARGS(&cmd_allocator)));

    device.GetDevice()->CreateCommandList(
        0,
        to_d3d_command_type(type),
        cmd_allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&cmd_list));

    if (!name.empty()) {
        ThrowIfFailed(cmd_list->SetName(std::pmr::wstring(name.begin(), name.end()).data()));
        ThrowIfFailed(cmd_allocator->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
    }
};

DX12CommandContext::DX12CommandContext(DX12Device& device, CommandType type, std::string_view name, std::uint64_t& fence_value)
    : m_Device(device),
      m_Type(type),
      m_ResourceBinder(*this),
      m_FenceValue(fence_value) {
    initialize_command_context(device, type, name, m_CmdAllocator, m_CmdList);
}

void DX12CommandContext::FlushBarriers() {
    if (!m_Barriers.empty()) {
        m_CmdList->ResourceBarrier(m_Barriers.size(), m_Barriers.data());
        m_Barriers.clear();
    }
}

DX12GraphicsCommandContext::DX12GraphicsCommandContext(DX12Device& device, std::string_view name)
    : GraphicsCommandContext(device, CommandType::Graphics, name),
      DX12CommandContext(device, CommandType::Graphics, name, fence_value) {
}

DX12ComputeCommandContext::DX12ComputeCommandContext(DX12Device& device, std::string_view name)
    : ComputeCommandContext(device, CommandType::Compute, name),
      DX12CommandContext(device, CommandType::Compute, name, fence_value) {
}

DX12CopyCommandContext::DX12CopyCommandContext(DX12Device& device, std::string_view name)
    : CopyCommandContext(device, CommandType::Copy, name),
      DX12CommandContext(device, CommandType::Copy, name, fence_value) {
}

void DX12GraphicsCommandContext::SetName(std::string_view name) {
    m_Name = name;
    ThrowIfFailed(m_CmdList->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
    ThrowIfFailed(m_CmdAllocator->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
}

void DX12ComputeCommandContext::SetName(std::string_view name) {
    m_Name = name;
    ThrowIfFailed(m_CmdList->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
    ThrowIfFailed(m_CmdAllocator->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
}

void DX12CopyCommandContext::SetName(std::string_view name) {
    m_Name = name;
    ThrowIfFailed(m_CmdList->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
    ThrowIfFailed(m_CmdAllocator->SetName(std::pmr::wstring(name.begin(), name.end()).c_str()));
}

void DX12GraphicsCommandContext::ResetState(GpuBuffer& buffer) {
    TransitionResource(buffer, D3D12_RESOURCE_STATE_COMMON);
}

void DX12GraphicsCommandContext::ResetState(Texture& texture) {
    TransitionResource(texture, D3D12_RESOURCE_STATE_COMMON);
}

void DX12ComputeCommandContext::ResetState(GpuBuffer& buffer) {
    TransitionResource(buffer, D3D12_RESOURCE_STATE_COMMON);
}

void DX12ComputeCommandContext::ResetState(Texture& texture) {
    TransitionResource(texture, D3D12_RESOURCE_STATE_COMMON);
}

void DX12CopyCommandContext::ResetState(GpuBuffer& buffer) {
    TransitionResource(buffer, D3D12_RESOURCE_STATE_COMMON);
}

void DX12CopyCommandContext::ResetState(Texture& texture) {
    TransitionResource(texture, D3D12_RESOURCE_STATE_COMMON);
}

void DX12GraphicsCommandContext::Reset() {
    ThrowIfFailed(m_CmdAllocator->Reset());
    m_ResourceBinder.Reset();
    ThrowIfFailed(m_CmdList->Reset(m_CmdAllocator.Get(), nullptr));
}

void DX12ComputeCommandContext::Reset() {
    ThrowIfFailed(m_CmdAllocator->Reset());
    m_ResourceBinder.Reset();
    m_CmdList->Reset(m_CmdAllocator.Get(), nullptr);
}

void DX12CopyCommandContext::Reset() {
    ThrowIfFailed(m_CmdAllocator->Reset());
    m_ResourceBinder.Reset();
    ThrowIfFailed(m_CmdList->Reset(m_CmdAllocator.Get(), nullptr));
}

void DX12GraphicsCommandContext::End() {
    FlushBarriers();
    ThrowIfFailed(m_CmdList->Close());
}

void DX12ComputeCommandContext::End() {
    FlushBarriers();
    ThrowIfFailed(m_CmdList->Close());
}

void DX12CopyCommandContext::End() {
    FlushBarriers();
    ThrowIfFailed(m_CmdList->Close());
}

void DX12GraphicsCommandContext::SetViewPort(const ViewPort& view_port) {
    const auto d3d_view_port = to_d3d_view_port(view_port);
    m_CmdList->RSSetViewports(1, &d3d_view_port);
}

void DX12GraphicsCommandContext::SetScissorRect(const Rect& scissor_rect) {
    const auto d3d_rect = to_d3d_rect(scissor_rect);
    m_CmdList->RSSetScissorRects(1, &d3d_rect);
}

void DX12GraphicsCommandContext::SetBlendColor(const math::vec4f& color) {
    m_CmdList->OMSetBlendFactor(color);
}

void DX12GraphicsCommandContext::SetRenderTarget(const TextureView& target) {
    TransitionResource(target.desc.textuer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    const auto& rtv = static_cast<const DX12DescriptorWrapper<TextureView>&>(target).rtv;
    m_CmdList->OMSetRenderTargets(1, &rtv.cpu_handle, false, nullptr);
}

void DX12GraphicsCommandContext::SetRenderTargetAndDepthStencil(const TextureView& target, const TextureView& depth_stencil) {
    TransitionResource(target.desc.textuer, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(depth_stencil.desc.textuer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

    const auto& dx12_texture_view = static_cast<const DX12DescriptorWrapper<TextureView>&>(target);
    m_CmdList->OMSetRenderTargets(1, &dx12_texture_view.rtv.cpu_handle, false, &dx12_texture_view.dsv.cpu_handle);
}

void DX12GraphicsCommandContext::ClearRenderTarget(const TextureView& target) {
    const auto& clear_color = target.desc.textuer.desc.clear_value.color;

    const auto& rtv = static_cast<const DX12DescriptorWrapper<TextureView>&>(target).rtv;
    m_CmdList->ClearRenderTargetView(rtv.cpu_handle, clear_color, 0, nullptr);
}

void DX12GraphicsCommandContext::ClearDepthStencil(const TextureView& depth_stencil) {
    const auto& clear_value = depth_stencil.desc.textuer.desc.clear_value;

    const auto& dsv = static_cast<const DX12DescriptorWrapper<TextureView>&>(depth_stencil).dsv;
    m_CmdList->ClearDepthStencilView(dsv.cpu_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clear_value.depth, clear_value.stencil, 0, nullptr);
}

void DX12GraphicsCommandContext::SetPipeline(const RenderPipeline& pipeline) {
    auto& d3d_pipeline = static_cast<const DX12RenderPipeline&>(pipeline);
    m_ResourceBinder.SetRootSignature(&d3d_pipeline.root_signature_desc);
    m_CmdList->SetGraphicsRootSignature(d3d_pipeline.root_signature.Get());
    m_CmdList->SetPipelineState(d3d_pipeline.pso.Get());
    m_CmdList->IASetPrimitiveTopology(to_d3d_primitive_topology(pipeline.desc.topology));
}

void DX12GraphicsCommandContext::SetIndexBuffer(const GpuBufferView& buffer) {
    TransitionResource(buffer.desc.buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER, true);
    m_CmdList->IASetIndexBuffer(&static_cast<const DX12GpuBufferView&>(buffer).ibv.value());
}

void DX12GraphicsCommandContext::SetVertexBuffer(std::uint8_t slot, const GpuBufferView& buffer) {
    TransitionResource(buffer.desc.buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, true);
    m_CmdList->IASetVertexBuffers(slot, 1, &static_cast<const DX12GpuBufferView&>(buffer).vbv.value());
}

void DX12GraphicsCommandContext::PushConstant(std::uint32_t slot, const std::span<const std::byte>& data) {
    m_ResourceBinder.PushConstant(slot, data);
}

void DX12GraphicsCommandContext::BindConstantBuffer(std::uint32_t slot, const GpuBufferView& buffer, std::size_t index) {
    m_ResourceBinder.BindConstantBuffer(slot, buffer, index);
}

void DX12GraphicsCommandContext::BindTexture(std::uint32_t slot, const TextureView& texture) {
    m_ResourceBinder.BindTexture(slot, texture);
}

int DX12GraphicsCommandContext::GetBindless(const TextureView& texture_view) {
    return -1;
}

void DX12GraphicsCommandContext::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    m_ResourceBinder.FlushDescriptors();
    m_CmdList->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void DX12GraphicsCommandContext::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {
    m_ResourceBinder.FlushDescriptors();
    m_CmdList->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

void DX12GraphicsCommandContext::CopyTexture(const Texture& src, const Texture& dest) {
    auto& d3d_src_tex = static_cast<const DX12ResourceWrapper<Texture>&>(src);
    auto& d3d_dst_tex = static_cast<const DX12ResourceWrapper<Texture>&>(dest);

    D3D12_TEXTURE_COPY_LOCATION src_desc{
        .pResource        = d3d_src_tex.resource.Get(),
        .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    D3D12_TEXTURE_COPY_LOCATION dst_desc{
        .pResource        = d3d_dst_tex.resource.Get(),
        .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    D3D12_BOX region{
        .left   = 0,
        .top    = 0,
        .front  = 0,
        .right  = src.desc.width,
        .bottom = src.desc.height,
        .back   = src.desc.depth,
    };
    m_CmdList->CopyTextureRegion(&dst_desc, 0, 0, 0, &src_desc, &region);
}

void DX12GraphicsCommandContext::Present(Texture& back_buffer) {
    TransitionResource(back_buffer, D3D12_RESOURCE_STATE_PRESENT, true);
}

void DX12CopyCommandContext::CopyBuffer(const GpuBuffer& src, std::size_t src_offset, GpuBuffer& dest, std::size_t dest_offset, std::size_t size) {
    auto& src_res  = static_cast<const DX12ResourceWrapper<GpuBuffer>&>(src);
    auto& dest_res = static_cast<const DX12ResourceWrapper<GpuBuffer>&>(dest);

    m_CmdList->CopyBufferRegion(dest_res.resource.Get(), dest_offset, src_res.resource.Get(), src_offset, size);
}

void DX12CopyCommandContext::CopyTexture(const Texture& src, const Texture& dest) {
    auto& d3d_src_tex = static_cast<const DX12ResourceWrapper<Texture>&>(src);
    auto& d3d_dst_tex = static_cast<const DX12ResourceWrapper<Texture>&>(dest);

    D3D12_TEXTURE_COPY_LOCATION src_desc{
        .pResource        = d3d_src_tex.resource.Get(),
        .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    D3D12_TEXTURE_COPY_LOCATION dst_desc{
        .pResource        = d3d_dst_tex.resource.Get(),
        .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    D3D12_BOX region{
        .left   = 0,
        .top    = 0,
        .front  = 0,
        .right  = src.desc.width,
        .bottom = src.desc.height,
        .back   = src.desc.depth,
    };
    m_CmdList->CopyTextureRegion(&dst_desc, 0, 0, 0, &src_desc, &region);
}

}  // namespace hitagi::gfx