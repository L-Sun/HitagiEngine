#include "dx12_command_list.hpp"
#include "dx12_resource.hpp"
#include "dx12_bindless.hpp"
#include "dx12_device.hpp"
#include "dx12_utils.hpp"

#include <hitagi/utils/exceptions.hpp>

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {
auto initialize_command_context(DX12Device& device, CommandType type, ComPtr<ID3D12CommandAllocator>& cmd_allocator, ComPtr<ID3D12GraphicsCommandList>& cmd_list, std::string_view name) {
    const auto logger = device.GetLogger();

    if (FAILED(device.GetDevice()->CreateCommandAllocator(to_d3d_command_type(type), IID_PPV_ARGS(&cmd_allocator)))) {
        const auto error_message = fmt::format("failed to create command allocator({})", fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    {
        const auto allocator_name = fmt::format("CommandAllocator({})", name);
        cmd_allocator->SetName(std::wstring(allocator_name.begin(), allocator_name.end()).c_str());
    }

    if (FAILED(device.GetDevice()->CreateCommandList(0, to_d3d_command_type(type), cmd_allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list)))) {
        const auto error_message = fmt::format("failed to create command list({})", fmt::styled(name, fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    cmd_list->SetName(std::wstring(name.begin(), name.end()).c_str());
};

inline void pipeline_barrier_fn(const ComPtr<ID3D12GraphicsCommandList7>& command_list,
                                const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    // convert to vulkan barriers
    std::pmr::vector<D3D12_GLOBAL_BARRIER>  dx12_global_barriers;
    std::pmr::vector<D3D12_BUFFER_BARRIER>  dx12_buffer_barriers;
    std::pmr::vector<D3D12_TEXTURE_BARRIER> dx12_texture_barriers;

    std::transform(global_barriers.begin(), global_barriers.end(), std::back_inserter(dx12_global_barriers), to_d3d_global_barrier);
    std::transform(buffer_barriers.begin(), buffer_barriers.end(), std::back_inserter(dx12_buffer_barriers), to_d3d_buffer_barrier);
    std::transform(texture_barriers.begin(), texture_barriers.end(), std::back_inserter(dx12_texture_barriers), to_d3d_texture_barrier);

    const std::array barrier_groups = {
        D3D12_BARRIER_GROUP{
            .Type            = D3D12_BARRIER_TYPE_GLOBAL,
            .NumBarriers     = static_cast<std::uint32_t>(dx12_global_barriers.size()),
            .pGlobalBarriers = dx12_global_barriers.data(),
        },
        D3D12_BARRIER_GROUP{
            .Type            = D3D12_BARRIER_TYPE_BUFFER,
            .NumBarriers     = static_cast<std::uint32_t>(dx12_buffer_barriers.size()),
            .pBufferBarriers = dx12_buffer_barriers.data(),
        },
        D3D12_BARRIER_GROUP{
            .Type             = D3D12_BARRIER_TYPE_TEXTURE,
            .NumBarriers      = static_cast<std::uint32_t>(dx12_texture_barriers.size()),
            .pTextureBarriers = dx12_texture_barriers.data(),
        },
    };

    command_list->Barrier(barrier_groups.size(), barrier_groups.data());
}

DX12GraphicsCommandList::DX12GraphicsCommandList(DX12Device& device, std::string_view name)
    : GraphicsCommandContext(device, name) {
    initialize_command_context(device, CommandType::Graphics, command_allocator, command_list, name);
}

void DX12GraphicsCommandList::Begin() {
    auto& dx12_bindless_utils = static_cast<DX12BindlessUtils&>(m_Device.GetBindlessUtils());
    auto  descriptor_heaps    = dx12_bindless_utils.GetDescriptorHeaps();
    command_list->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
    command_list->SetGraphicsRootSignature(dx12_bindless_utils.GetBindlessRootSignature().Get());

    m_Pipeline = nullptr;
}

void DX12GraphicsCommandList::End() {
    command_list->Close();
}

void DX12GraphicsCommandList::ResourceBarrier(const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                              const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                              const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (FAILED(command_list.As(&cmd_list))) {
        const auto error_message = fmt::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12GraphicsCommandList::BeginRendering(Texture& render_target, utils::optional_ref<Texture> depth_stencil) {
    auto& dx12_render_target = static_cast<DX12Texture&>(render_target);
    auto  dx12_depth_stencil = depth_stencil.has_value() ? &static_cast<DX12Texture&>(depth_stencil->get()) : nullptr;

    command_list->OMSetRenderTargets(
        1, &dx12_render_target.rtv.GetCPUHandle(), false,
        (dx12_depth_stencil && dx12_depth_stencil->dsv) ? &dx12_depth_stencil->dsv.GetCPUHandle() : nullptr);

    if (render_target.GetDesc().clear_value.has_value()) {
        const auto& clear_color = std::get<ClearColor>(render_target.GetDesc().clear_value.value());
        command_list->ClearRenderTargetView(dx12_render_target.rtv.GetCPUHandle(), clear_color, 0, nullptr);
    }
    if (dx12_depth_stencil && dx12_depth_stencil->GetDesc().clear_value.has_value()) {
        const auto& clear_depth_stencil = std::get<ClearDepthStencil>(dx12_depth_stencil->GetDesc().clear_value.value());
        command_list->ClearDepthStencilView(dx12_depth_stencil->dsv.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clear_depth_stencil.depth, clear_depth_stencil.stencil, 0, nullptr);
    }
}

void DX12GraphicsCommandList::EndRendering() {}

void DX12GraphicsCommandList::SetPipeline(const RenderPipeline& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;

    auto& dx12_pipeline = dynamic_cast<const DX12RenderPipeline&>(pipeline);
    command_list->SetPipelineState(dx12_pipeline.pipeline.Get());
    command_list->IASetPrimitiveTopology(to_d3d_primitive_topology(dx12_pipeline.GetDesc().assembly_state.primitive));
}

void DX12GraphicsCommandList::SetViewPort(const ViewPort& view_port) {
    const auto d3d_view_port = to_d3d_view_port(view_port);
    command_list->RSSetViewports(1, &d3d_view_port);
}

void DX12GraphicsCommandList::SetScissorRect(const Rect& scissor_rect) {
    const auto d3d_scissor_rect = to_d3d_rect(scissor_rect);
    command_list->RSSetScissorRects(1, &d3d_scissor_rect);
}

void DX12GraphicsCommandList::SetBlendColor(const math::vec4f& color) {
    command_list->OMSetBlendFactor(color);
}

void DX12GraphicsCommandList::SetIndexBuffer(GPUBuffer& buffer) {
    auto&                   dx12_buffer = dynamic_cast<DX12GPUBuffer&>(buffer);
    D3D12_INDEX_BUFFER_VIEW ibv{
        .BufferLocation = dx12_buffer.resource->GetGPUVirtualAddress(),
        .SizeInBytes    = static_cast<UINT>(buffer.Size()),
        .Format         = buffer.GetDesc().element_size == sizeof(std::uint16_t) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
    };
    command_list->IASetIndexBuffer(&ibv);
}

void DX12GraphicsCommandList::SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) {
    if (m_Pipeline == nullptr) {
        const auto error_message = fmt::format("pipeline is not set");
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }

    auto& dx12_buffer = dynamic_cast<DX12GPUBuffer&>(buffer);

    auto& input_layout = m_Pipeline->GetDesc().vertex_input_layout;
    if (auto iter = std::find_if(
            input_layout.begin(), input_layout.end(),
            [slot](const auto& attr) { return attr.binding == slot; });
        iter != input_layout.end()) {
        D3D12_VERTEX_BUFFER_VIEW vbv{
            .BufferLocation = dx12_buffer.resource->GetGPUVirtualAddress(),
            .SizeInBytes    = static_cast<UINT>(buffer.Size()),
            .StrideInBytes  = static_cast<UINT>(iter->stride),
        };
        command_list->IASetVertexBuffers(slot, 1, &vbv);
    }
}

void DX12GraphicsCommandList::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    command_list->SetGraphicsRoot32BitConstants(0, sizeof(info) / sizeof(std::uint32_t), &info, 0);
}

void DX12GraphicsCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    command_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void DX12GraphicsCommandList::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {
    command_list->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

void DX12GraphicsCommandList::CopyTexture(const Texture& src, Texture& dest) {}

DX12ComputeCommandList::DX12ComputeCommandList(DX12Device& device, std::string_view name)
    : ComputeCommandContext(device, name) {
    initialize_command_context(device, CommandType::Compute, command_allocator, command_list, name);
}

void DX12ComputeCommandList::Begin() {
    auto& dx12_bindless_utils = static_cast<DX12BindlessUtils&>(m_Device.GetBindlessUtils());
    command_list->SetComputeRootSignature(dx12_bindless_utils.GetBindlessRootSignature().Get());

    m_Pipeline = nullptr;
}

void DX12ComputeCommandList::End() {
    command_list->Close();
}

void DX12ComputeCommandList::ResourceBarrier(const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                             const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                             const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (FAILED(command_list.As(&cmd_list))) {
        const auto error_message = fmt::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12ComputeCommandList::SetPipeline(const ComputePipeline& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;

    auto& dx12_pipeline = dynamic_cast<const DX12ComputePipeline&>(pipeline);
    command_list->SetPipelineState(dx12_pipeline.pipeline.Get());
}

void DX12ComputeCommandList::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    command_list->SetComputeRoot32BitConstants(0, sizeof(info) / sizeof(std::uint32_t), &info, 0);
}

DX12CopyCommandList::DX12CopyCommandList(DX12Device& device, std::string_view name)
    : CopyCommandContext(device, name) {
    initialize_command_context(device, CommandType::Copy, command_allocator, command_list, name);
}

void DX12CopyCommandList::Begin() {
}

void DX12CopyCommandList::End() {
    command_list->Close();
}

void DX12CopyCommandList::ResourceBarrier(
    const std::pmr::vector<GlobalBarrier>&    global_barriers,
    const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
    const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (FAILED(command_list.As(&cmd_list))) {
        const auto error_message = fmt::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12CopyCommandList::CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) {
    auto dx12_src = dynamic_cast<const DX12GPUBuffer&>(src).resource.Get();
    auto dx12_dst = dynamic_cast<DX12GPUBuffer&>(dst).resource.Get();

    command_list->CopyBufferRegion(dx12_dst, dst_offset, dx12_src, src_offset, size);
}

void DX12CopyCommandList::CopyTexture(const Texture& src, Texture& dst) {
    throw utils::NoImplemented();
}

void DX12CopyCommandList::CopyBufferToTexture(const GPUBuffer& src,
                                              std::size_t      src_offset,
                                              Texture&         dst,
                                              math::vec3i      dst_offset,
                                              math::vec3u      extent,
                                              std::uint32_t    mip_level,
                                              std::uint32_t    base_array_layer,
                                              std::uint32_t    layer_count) {
    const auto& dx12_src_buffer  = dynamic_cast<const DX12GPUBuffer&>(src);
    auto&       dx12_dst_texture = dynamic_cast<DX12Texture&>(dst);

    const CD3DX12_TEXTURE_COPY_LOCATION src_location(
        dx12_src_buffer.resource.Get(),
        {
            .Offset    = src_offset,
            .Footprint = {
                .Format   = to_dxgi_format(dx12_dst_texture.GetDesc().format),
                .Width    = extent.x,
                .Height   = extent.y,
                .Depth    = extent.z,
                .RowPitch = static_cast<UINT>(dx12_dst_texture.GetDesc().width * get_format_byte_size(dx12_dst_texture.GetDesc().format)),
            },
        });
    const CD3DX12_TEXTURE_COPY_LOCATION dst_location(dx12_dst_texture.resource.Get(), D3D12CalcSubresource(mip_level, base_array_layer, 0, dx12_dst_texture.GetDesc().mip_levels, layer_count));

    const CD3DX12_BOX src_box(dst_offset.x, dst_offset.y, dst_offset.z, dst_offset.x + extent.x, dst_offset.y + extent.y, dst_offset.z + extent.z);

    command_list->CopyTextureRegion(
        &dst_location, dst_offset.x, dst_offset.y, dst_offset.z,
        &src_location, &src_box);
}

}  // namespace hitagi::gfx