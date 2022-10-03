#include "dx12_command_context.hpp"
#include <d3d12.h>
#include "dx12_device.hpp"
#include "dx12_resource.hpp"
#include "utils.hpp"

namespace hitagi::gfx {
template <typename T>
auto initialize_command_context(DX12Device* device, CommandType type, std::string_view name, ComPtr<ID3D12CommandAllocator> cmd_allocator, ComPtr<T>& cmd_list) {
    ThrowIfFailed(device->GetDevice()->CreateCommandAllocator(
        to_d3d_command_type(type),
        IID_PPV_ARGS(&cmd_allocator)));

    device->GetDevice()->CreateCommandList(
        0,
        to_d3d_command_type(type),
        cmd_allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&cmd_list));

    if (!name.empty())
        cmd_list->SetName(std::wstring(name.begin(), name.end()).data());
};

DX12CommandContext::DX12CommandContext(DX12Device* device, CommandType type, std::string_view name)
    : m_Device(device),
      m_Type(type),
      m_ResourceBinder(*this) {
    initialize_command_context(device, type, name, m_CmdAllocator, m_CmdList);
}

void DX12CommandContext::FlushBarriers() {
    if (!m_Barriers.empty()) {
        m_CmdList->ResourceBarrier(m_Barriers.size(), m_Barriers.data());
        m_Barriers.clear();
    }
}

DX12GraphicsCommandContext::DX12GraphicsCommandContext(DX12Device* device, std::string_view name)
    : GraphicsCommandContext(device, CommandType::Graphics, name),
      DX12CommandContext(device, CommandType::Graphics, name) {
}

DX12ComputeCommandContext::DX12ComputeCommandContext(DX12Device* device, std::string_view name)
    : ComputeCommandContext(device, CommandType::Compute, name),
      DX12CommandContext(device, CommandType::Compute, name) {
}

DX12CopyCommandContext::DX12CopyCommandContext(DX12Device* device, std::string_view name)
    : CopyCommandContext(device, CommandType::Copy, name),
      DX12CommandContext(device, CommandType::Copy, name) {
}

void DX12GraphicsCommandContext::End() {
    ThrowIfFailed(m_CmdList->Close());
}

void DX12ComputeCommandContext::End() {
    ThrowIfFailed(m_CmdList->Close());
}

void DX12CopyCommandContext::End() {
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
    auto logger = static_cast<DX12Device*>(device)->GetLogger();

    auto target_resource = std::static_pointer_cast<DX12ResourceWrapper<Texture>>(target.desc.textuer);
    TransitionResource(*target_resource, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    const auto& rtv = static_cast<const DX12DescriptorWrapper<TextureView>&>(target).rtv;
    m_CmdList->OMSetRenderTargets(1, &rtv.cpu_handle, false, nullptr);
}

void DX12GraphicsCommandContext::SetRenderTargetAndDepthStencil(const TextureView& target, const TextureView& depth_stencil) {
    auto target_resource        = std::static_pointer_cast<DX12ResourceWrapper<Texture>>(target.desc.textuer);
    auto depth_stencil_resource = std::static_pointer_cast<DX12ResourceWrapper<Texture>>(depth_stencil.desc.textuer);

    TransitionResource(*target_resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(*depth_stencil_resource, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

    const auto& dx12_texture_view = static_cast<const DX12DescriptorWrapper<TextureView>&>(target);
    m_CmdList->OMSetRenderTargets(1, &dx12_texture_view.rtv.cpu_handle, false, &dx12_texture_view.dsv.cpu_handle);
}

void DX12GraphicsCommandContext::ClearRenderTarget(const TextureView& target) {
    const auto& clear_color = target.desc.textuer->desc.clear_value.color;

    const auto& rtv = static_cast<const DX12DescriptorWrapper<TextureView>&>(target).rtv;
    m_CmdList->ClearRenderTargetView(rtv.cpu_handle, clear_color, 0, nullptr);
}

void DX12GraphicsCommandContext::ClearDepthStencil(const TextureView& depth_stencil) {
    const auto& clear_value = depth_stencil.desc.textuer->desc.clear_value;

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
    m_CmdList->IASetIndexBuffer(&static_cast<const DX12GpuBufferView&>(buffer).ibv.value());
}

void DX12GraphicsCommandContext::SetVertexBuffer(std::uint8_t slot, const GpuBufferView& buffer) {
    m_CmdList->IASetVertexBuffers(slot, 1, &static_cast<const DX12GpuBufferView&>(buffer).vbv.value());
}

void DX12GraphicsCommandContext::PushConstant(std::uint32_t slot, const std::span<const std::byte>& data) {
    m_ResourceBinder.PushConstant(slot, data);
}

void DX12GraphicsCommandContext::BindConstantBuffer(std::uint32_t slot, const GpuBufferView& buffer, std::size_t index) {
    m_ResourceBinder.BindConstantBuffer(slot, buffer, index);
}

int DX12GraphicsCommandContext::GetBindless(const TextureView& texture_view) {
}

void DX12GraphicsCommandContext::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    m_ResourceBinder.FlushDescriptors();
    m_CmdList->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void DX12GraphicsCommandContext::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {
    m_ResourceBinder.FlushDescriptors();
    m_CmdList->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

void DX12GraphicsCommandContext::Present(const TextureView& render_target) {
    auto target_resource = std::static_pointer_cast<DX12ResourceWrapper<Texture>>(render_target.desc.textuer);
    TransitionResource(*target_resource, D3D12_RESOURCE_STATE_PRESENT, true);
}

void DX12CopyCommandContext::CopyBuffer(const GpuBuffer& src, std::size_t src_offset, GpuBuffer& dest, std::size_t dest_offset, std::size_t size) {
    auto& src_res  = static_cast<const DX12ResourceWrapper<GpuBuffer>&>(src);
    auto& dest_res = static_cast<const DX12ResourceWrapper<GpuBuffer>&>(dest);

    m_CmdList->CopyBufferRegion(dest_res.resource.Get(), dest_offset, src_res.resource.Get(), src_offset, size);
}

}  // namespace hitagi::gfx