module;
#include "interop/win32_macros.hpp"
#include "interop/tracy_macros.hpp"
#include <cstring>

export module gfx.dx12:command_list;
#ifdef _WIN32
import interop.win32;
#endif
import interop.fmt;
import interop.spdlog;
import interop.tracy.dx12;
import interop.tracy;
import interop.dx12;
import interop.magic_enum;

import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :bindless;
import :utils;
import :resource;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

class DX12GraphicsCommandList : public GraphicsCommandContext {
public:
    DX12GraphicsCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name);
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final;

    void BeginRendering(TextureView&                     render_target,
                        utils::optional_ref<TextureView> depth_stencil       = {},
                        bool                             clear_render_target = false,
                        bool                             clear_depth_stencil = false) final;
    void EndRendering() final;

    void SetPipeline(const RenderPipeline& pipeline) final;

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::Color& color) final;

    void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset = 0, Format index_format = Format::R32_UINT) final;
    void SetVertexBuffers(
        std::uint8_t                                             start_binding,
        std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
        std::span<const std::size_t>                             offsets) final;

    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final;

    void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) final;

    ComPtr<ID3D12GraphicsCommandList>      command_list;
    ComPtr<ID3D12CommandAllocator>         command_allocator;
    const RenderPipeline*                  m_Pipeline = nullptr;
    std::unique_ptr<tracy::D3D12ZoneScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyD3D12Ctx                   m_TracyCtx;
    DX12BindlessUtils&              m_BindlessUtils;
};

class DX12ComputeCommandList : public ComputeCommandContext {
public:
    DX12ComputeCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name);
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final;

    void SetPipeline(const ComputePipeline& pipeline) final;
    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    ComPtr<ID3D12GraphicsCommandList>      command_list;
    ComPtr<ID3D12CommandAllocator>         command_allocator;
    const ComputePipeline*                 m_Pipeline = nullptr;
    std::unique_ptr<tracy::D3D12ZoneScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyD3D12Ctx                   m_TracyCtx;
    DX12BindlessUtils&              m_BindlessUtils;
};

class DX12CopyCommandList : public CopyCommandContext {
public:
    DX12CopyCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, TracyD3D12Ctx tracy_context, std::string_view name);
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final;

    void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) final;
    void CopyBufferToTexture(
        const GPUBuffer&        src,
        std::size_t             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer dst_layer = {}) final;

    void CopyTextureToBuffer(
        const Texture&          src,
        math::vec3i             src_offset,
        math::vec3u             extent,
        GPUBuffer&              dst,
        std::size_t             dst_offset,
        TextureSubresourceLayer src_layer = {}) final;

    void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) final;

    ComPtr<ID3D12GraphicsCommandList>      command_list;
    ComPtr<ID3D12CommandAllocator>         command_allocator;
    std::unique_ptr<tracy::D3D12ZoneScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyD3D12Ctx                   m_TracyCtx;
};

inline auto to_d3d_buffer_barrier(GPUBufferBarrier barrier) noexcept -> D3D12_BUFFER_BARRIER {
    return {
        .SyncBefore   = to_d3d_pipeline_stage(barrier.src_stage),
        .SyncAfter    = to_d3d_pipeline_stage(barrier.dst_stage),
        .AccessBefore = to_d3d_barrier_access(barrier.src_access),
        .AccessAfter  = to_d3d_barrier_access(barrier.dst_access),
        .pResource    = dynamic_cast<DX12GPUBuffer&>(barrier.buffer).resource.Get(),
        .Offset       = 0,
        .Size         = barrier.buffer.Size(),
    };
}

inline auto to_d3d_texture_barrier(TextureBarrier barrier) noexcept -> D3D12_TEXTURE_BARRIER {
    return {
        .SyncBefore   = to_d3d_pipeline_stage(barrier.src_stage),
        .SyncAfter    = to_d3d_pipeline_stage(barrier.dst_stage),
        .AccessBefore = to_d3d_barrier_access(barrier.src_access),
        .AccessAfter  = to_d3d_barrier_access(barrier.dst_access),
        .LayoutBefore = to_d3d_texture_layout(barrier.src_layout),
        .LayoutAfter  = to_d3d_texture_layout(barrier.dst_layout),
        .pResource    = dynamic_cast<DX12Texture&>(barrier.texture).resource.Get(),
        .Subresources = {
            .IndexOrFirstMipLevel = 0,
            .NumMipLevels         = barrier.texture.GetDesc().mip_levels,
            .FirstArraySlice      = 0,
            .NumArraySlices       = barrier.texture.GetDesc().array_size,
            .FirstPlane           = 0,
            .NumPlanes            = 1,
        },
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

auto initialize_command_context(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, CommandType type, ComPtr<ID3D12CommandAllocator>& cmd_allocator, ComPtr<ID3D12GraphicsCommandList>& cmd_list, std::string_view name) {
    if (interop::failed(device.CreateCommandAllocator(to_d3d_command_type(type), IID_PPV_ARGS(&cmd_allocator)))) {
        const auto error_message = fmt::format("failed to create command allocator({})", fmt::styled(name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    {
        const auto allocator_name = std::format("CommandAllocator({})", name);
        cmd_allocator->SetName(std::wstring(allocator_name.begin(), allocator_name.end()).c_str());
    }

    if (interop::failed(device.CreateCommandList(0, to_d3d_command_type(type), cmd_allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list)))) {
        const auto error_message = fmt::format("failed to create command list({})", fmt::styled(name, fmt::fg(fmt::color::green)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    cmd_list->SetName(std::wstring(name.begin(), name.end()).c_str());
};

inline void pipeline_barrier_fn(const ComPtr<ID3D12GraphicsCommandList7>& command_list,
                                std::span<const GlobalBarrier>            global_barriers,
                                std::span<const GPUBufferBarrier>         buffer_barriers,
                                std::span<const TextureBarrier>           texture_barriers) {
    const auto dx12_global_barriers  = global_barriers | std::ranges::views::transform(to_d3d_global_barrier) | std::ranges::to<std::pmr::vector<D3D12_GLOBAL_BARRIER>>();
    const auto dx12_buffer_barriers  = buffer_barriers | std::ranges::views::transform(to_d3d_buffer_barrier) | std::ranges::to<std::pmr::vector<D3D12_BUFFER_BARRIER>>();
    const auto dx12_texture_barriers = texture_barriers |
                                       std::ranges::views::transform(to_d3d_texture_barrier) |
                                       std::ranges::views::transform([&](auto&& dx12_barrier) {
                                           // Copy queues only support COMMON and UNDEFINED layouts for texture barriers
                                           if (command_list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY) {
                                               if (dx12_barrier.LayoutBefore != D3D12_BARRIER_LAYOUT_UNDEFINED)
                                                   dx12_barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
                                               if (dx12_barrier.LayoutAfter != D3D12_BARRIER_LAYOUT_UNDEFINED)
                                                   dx12_barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_COMMON;
                                           }
                                           return dx12_barrier;
                                       }) |
                                       std::ranges::to<std::pmr::vector<D3D12_TEXTURE_BARRIER>>();

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

inline void copy_texture_region(const ComPtr<ID3D12GraphicsCommandList>& command_list,
                                const Texture&                           src,
                                math::vec3i                              src_offset,
                                Texture&                                 dst,
                                math::vec3i                              dst_offset,
                                math::vec3u                              extent,
                                TextureSubresourceLayer                  src_layer,
                                TextureSubresourceLayer                  dst_layer) {
    auto& dx12_src_texture = static_cast<const DX12Texture&>(src);
    auto& dx12_dst_texture = static_cast<DX12Texture&>(dst);

    const CD3DX12_TEXTURE_COPY_LOCATION src_location(dx12_src_texture.resource.Get(), D3D12CalcSubresource(src_layer.mip_level, src_layer.base_array_layer, 0, dx12_src_texture.GetDesc().mip_levels, src_layer.layer_count));
    const CD3DX12_TEXTURE_COPY_LOCATION dst_location(dx12_dst_texture.resource.Get(), D3D12CalcSubresource(dst_layer.mip_level, dst_layer.base_array_layer, 0, dx12_dst_texture.GetDesc().mip_levels, dst_layer.layer_count));

    const CD3DX12_BOX src_box(src_offset.x, src_offset.y, src_offset.z, src_offset.x + extent.x, src_offset.y + extent.y, src_offset.z + extent.z);

    command_list->CopyTextureRegion(&dst_location, dst_offset.x, dst_offset.y, dst_offset.z, &src_location, &src_box);
}

DX12GraphicsCommandList::DX12GraphicsCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name)
    : GraphicsCommandContext(name), m_Logger(logger), m_TracyCtx(tracy_context), m_BindlessUtils(bindings) {
    initialize_command_context(device, logger, CommandType::Graphics, command_allocator, command_list, name);
}

void DX12GraphicsCommandList::Begin() {
    auto& dx12_bindless_utils = m_BindlessUtils;
    auto  descriptor_heaps    = dx12_bindless_utils.GetDescriptorHeaps();
    command_list->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
    command_list->SetGraphicsRootSignature(dx12_bindless_utils.GetBindlessRootSignature().Get());

#ifdef TRACY_ENABLE
    m_TracyZone = std::make_unique<tracy::D3D12ZoneScope>(
        m_TracyCtx,
        __LINE__,
        __FILE__,
        sizeof(__FILE__) - 1,
        __FUNCTION__,
        std::strlen(__FUNCTION__),
        m_Name.data(),
        m_Name.size(),
        command_list.Get(),
        true);
#endif

    m_Pipeline = nullptr;
}

void DX12GraphicsCommandList::End() {
    m_TracyZone.reset();
    command_list->Close();
}

void DX12GraphicsCommandList::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                              std::span<const GPUBufferBarrier> buffer_barriers,
                                              std::span<const TextureBarrier>   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (interop::failed(command_list.As(&cmd_list))) {
        const auto error_message = std::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12GraphicsCommandList::BeginRendering(TextureView& render_target, utils::optional_ref<TextureView> depth_stencil, bool clear_render_target, bool clear_depth_stencil) {
    auto& render_target_texture = *render_target.GetDesc().texture;
    auto& dx12_render_target    = static_cast<DX12TextureView&>(render_target);
    auto  dx12_depth_stencil    = depth_stencil.has_value() ? &static_cast<DX12TextureView&>(depth_stencil->get()) : nullptr;
    auto  depth_stencil_texture = depth_stencil.has_value() ? depth_stencil->get().GetDesc().texture.get() : nullptr;

    command_list->OMSetRenderTargets(
        1, &dx12_render_target.rtv.GetCPUHandle(), false,
        (dx12_depth_stencil && dx12_depth_stencil->dsv) ? &dx12_depth_stencil->dsv.GetCPUHandle() : nullptr);

    if (clear_render_target) {
        if (!render_target_texture.GetDesc().clear_value.has_value()) {
            m_Logger->warn(fmt::format(
                "render target({}) has no clear value but clear render target is requested",
                fmt::styled(render_target_texture.GetName(), fmt::fg(fmt::color::orange))));

        } else {
            const auto clear_color = std::get<ClearColor>(render_target_texture.GetDesc().clear_value.value());
            command_list->ClearRenderTargetView(dx12_render_target.rtv.GetCPUHandle(), clear_color, 0, nullptr);
        }
    }

    if (clear_depth_stencil) {
        if (dx12_depth_stencil == nullptr) {
            m_Logger->warn("depth stencil is not set but clear depth stencil is requested");
        } else if (!depth_stencil_texture->GetDesc().clear_value.has_value()) {
            m_Logger->warn(fmt::format(
                "depth stencil({}) has no clear value but clear depth stencil is requested",
                fmt::styled(depth_stencil_texture->GetName(), fmt::fg(fmt::color::orange))));
        } else {
            const auto clear_depth_stencil = std::get<ClearDepthStencil>(depth_stencil_texture->GetDesc().clear_value.value());
            command_list->ClearDepthStencilView(dx12_depth_stencil->dsv.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clear_depth_stencil.depth, clear_depth_stencil.stencil, 0, nullptr);
        }
    }
}

void DX12GraphicsCommandList::EndRendering() {}

void DX12GraphicsCommandList::SetPipeline(const RenderPipeline& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;

    auto& dx12_pipeline = static_cast<const DX12RenderPipeline&>(pipeline);
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

void DX12GraphicsCommandList::SetBlendColor(const math::Color& color) {
    command_list->OMSetBlendFactor(color);
}

void DX12GraphicsCommandList::SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset, Format index_format) {
    auto&                   dx12_buffer = static_cast<const DX12GPUBuffer&>(buffer);
    D3D12_INDEX_BUFFER_VIEW ibv{
        .BufferLocation = dx12_buffer.resource->GetGPUVirtualAddress() + offset,
        .SizeInBytes    = static_cast<UINT>(buffer.Size() - offset),
        .Format         = to_dxgi_format(index_format),
    };
    command_list->IASetIndexBuffer(&ibv);
}

void DX12GraphicsCommandList::SetVertexBuffers(std::uint8_t                                             start_binding,
                                               std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
                                               std::span<const std::size_t>                             offsets) {
    if (m_Pipeline == nullptr) {
        const auto error_message = std::format("pipeline is not set");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    auto& input_layout = m_Pipeline->GetDesc().vertex_input_layout;

    std::pmr::vector<D3D12_VERTEX_BUFFER_VIEW> vbvs;

    for (std::uint8_t buffer_index = 0; buffer_index < buffers.size(); buffer_index++) {
        std::uint8_t binding     = start_binding + buffer_index;
        auto         offset      = offsets[buffer_index];
        auto&        dx12_buffer = static_cast<const DX12GPUBuffer&>(buffers[buffer_index].get());

        if (auto iter = std::find_if(
                input_layout.begin(), input_layout.end(),
                [binding](const auto& attr) { return attr.binding == binding; });
            iter != input_layout.end()) {
            vbvs.emplace_back(D3D12_VERTEX_BUFFER_VIEW{
                .BufferLocation = dx12_buffer.resource->GetGPUVirtualAddress() + offset,
                .SizeInBytes    = static_cast<UINT>(dx12_buffer.Size()),
                .StrideInBytes  = static_cast<UINT>(iter->stride),
            });
        } else {
            const auto error_message = fmt::format(
                "missing binding({}) in pipeline({}) vertex input layout",
                fmt::styled(binding, fmt::fg(fmt::color::red)),
                fmt::styled(m_Pipeline->GetName(), fmt::fg(fmt::color::green)));
            m_Logger->error(error_message);
            throw std::runtime_error(error_message);
        }
    }
    command_list->IASetVertexBuffers(start_binding, vbvs.size(), vbvs.data());
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

void DX12GraphicsCommandList::CopyTextureRegion(const Texture&          src,
                                                math::vec3i             src_offset,
                                                Texture&                dst,
                                                math::vec3i             dst_offset,
                                                math::vec3u             extent,
                                                TextureSubresourceLayer src_layer,
                                                TextureSubresourceLayer dst_layer) {
    copy_texture_region(command_list, src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

DX12ComputeCommandList::DX12ComputeCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name)
    : ComputeCommandContext(name), m_Logger(logger), m_TracyCtx(tracy_context), m_BindlessUtils(bindings) {
    initialize_command_context(device, logger, CommandType::Compute, command_allocator, command_list, name);
}

void DX12ComputeCommandList::Begin() {
    auto& dx12_bindless_utils = m_BindlessUtils;
    auto  descriptor_heaps    = dx12_bindless_utils.GetDescriptorHeaps();
    command_list->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
    command_list->SetComputeRootSignature(dx12_bindless_utils.GetBindlessRootSignature().Get());

#ifdef TRACY_ENABLE
    m_TracyZone = std::make_unique<tracy::D3D12ZoneScope>(
        m_TracyCtx,
        __LINE__,
        __FILE__,
        sizeof(__FILE__) - 1,
        __FUNCTION__,
        std::strlen(__FUNCTION__),
        m_Name.data(),
        m_Name.size(),
        command_list.Get(),
        true);
#endif

    m_Pipeline = nullptr;
}

void DX12ComputeCommandList::End() {
    m_TracyZone.reset();
    command_list->Close();
}

void DX12ComputeCommandList::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                             std::span<const GPUBufferBarrier> buffer_barriers,
                                             std::span<const TextureBarrier>   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (interop::failed(command_list.As(&cmd_list))) {
        const auto error_message = std::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12ComputeCommandList::SetPipeline(const ComputePipeline& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;

    auto& dx12_pipeline = static_cast<const DX12ComputePipeline&>(pipeline);
    command_list->SetPipelineState(dx12_pipeline.pipeline.Get());
}

void DX12ComputeCommandList::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    command_list->SetComputeRoot32BitConstants(0, sizeof(info) / sizeof(std::uint32_t), &info, 0);
}

DX12CopyCommandList::DX12CopyCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, TracyD3D12Ctx tracy_context, std::string_view name)
    : CopyCommandContext(name), m_Logger(logger), m_TracyCtx(tracy_context) {
    initialize_command_context(device, logger, CommandType::Copy, command_allocator, command_list, name);
}

void DX12CopyCommandList::Begin() {
#ifdef TRACY_ENABLE
    m_TracyZone = std::make_unique<tracy::D3D12ZoneScope>(
        m_TracyCtx,
        __LINE__,
        __FILE__,
        sizeof(__FILE__) - 1,
        __FUNCTION__,
        std::strlen(__FUNCTION__),
        m_Name.data(),
        m_Name.size(),
        command_list.Get(),
        true);
#endif
}

void DX12CopyCommandList::End() {
    m_TracyZone.reset();
    command_list->Close();
}

void DX12CopyCommandList::ResourceBarrier(
    std::span<const GlobalBarrier>    global_barriers,
    std::span<const GPUBufferBarrier> buffer_barriers,
    std::span<const TextureBarrier>   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (interop::failed(command_list.As(&cmd_list))) {
        const auto error_message = std::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12CopyCommandList::CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) {
    auto dx12_src = static_cast<const DX12GPUBuffer&>(src).resource.Get();
    auto dx12_dst = static_cast<DX12GPUBuffer&>(dst).resource.Get();

    command_list->CopyBufferRegion(dx12_dst, dst_offset, dx12_src, src_offset, size);
}

void DX12CopyCommandList::CopyBufferToTexture(const GPUBuffer&        src,
                                              std::size_t             src_offset,
                                              Texture&                dst,
                                              math::vec3i             dst_offset,
                                              math::vec3u             extent,
                                              TextureSubresourceLayer dst_layer) {
    const auto& dx12_src_buffer  = static_cast<const DX12GPUBuffer&>(src);
    auto&       dx12_dst_texture = static_cast<DX12Texture&>(dst);

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
    const CD3DX12_TEXTURE_COPY_LOCATION dst_location(dx12_dst_texture.resource.Get(), D3D12CalcSubresource(dst_layer.mip_level, dst_layer.base_array_layer, 0, dx12_dst_texture.GetDesc().mip_levels, dst_layer.layer_count));

    const CD3DX12_BOX src_box(dst_offset.x, dst_offset.y, dst_offset.z, dst_offset.x + extent.x, dst_offset.y + extent.y, dst_offset.z + extent.z);

    command_list->CopyTextureRegion(
        &dst_location, dst_offset.x, dst_offset.y, dst_offset.z,
        &src_location, &src_box);
}

void DX12CopyCommandList::CopyTextureToBuffer(const Texture&          src,
                                              math::vec3i             src_offset,
                                              math::vec3u             extent,
                                              GPUBuffer&              dst,
                                              std::size_t             dst_offset,
                                              TextureSubresourceLayer src_layer) {
    const auto& dx12_src_texture = static_cast<const DX12Texture&>(src);
    auto&       dx12_dst_buffer  = static_cast<DX12GPUBuffer&>(dst);

    const CD3DX12_TEXTURE_COPY_LOCATION src_location(dx12_src_texture.resource.Get(), D3D12CalcSubresource(src_layer.mip_level, src_layer.base_array_layer, 0, dx12_src_texture.GetDesc().mip_levels, src_layer.layer_count));
    const CD3DX12_TEXTURE_COPY_LOCATION dst_location(
        dx12_dst_buffer.resource.Get(),
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT{
            .Offset    = dst_offset,
            .Footprint = {
                .Format   = to_dxgi_format(dx12_src_texture.GetDesc().format),
                .Width    = extent.x,
                .Height   = extent.y,
                .Depth    = extent.z,
                .RowPitch = static_cast<UINT>(extent.x * get_format_byte_size(dx12_src_texture.GetDesc().format)),
            },
        });

    const CD3DX12_BOX src_box(src_offset.x, src_offset.y, src_offset.z, src_offset.x + extent.x, src_offset.y + extent.y, src_offset.z + extent.z);

    command_list->CopyTextureRegion(
        &dst_location, 0, 0, 0,
        &src_location, &src_box);
}

void DX12CopyCommandList::CopyTextureRegion(const Texture&          src,
                                            math::vec3i             src_offset,
                                            Texture&                dst,
                                            math::vec3i             dst_offset,
                                            math::vec3u             extent,
                                            TextureSubresourceLayer src_layer,
                                            TextureSubresourceLayer dst_layer) {
    copy_texture_region(command_list, src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

}  // namespace hitagi::gfx
