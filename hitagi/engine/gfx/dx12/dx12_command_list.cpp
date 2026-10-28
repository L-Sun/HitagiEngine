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

// Backend-only storage. Wrappers remain short-lived; native objects survive them.
class DX12CommandStorage {
public:
    struct Entry {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        // One record per queue fence that actually submitted this entry since
        // its last acquisition, not per queue on the device or per Submit call.
        // Repeated submissions on the same queue retain only the maximum value;
        // every recorded fence must complete before the native objects are reused.
        std::pmr::vector<std::pair<ComPtr<ID3D12Fence>, std::uint64_t>> submissions;
        CommandType type;
        // Intrusive ownership link: returning a wrapper must not allocate.
        std::unique_ptr<Entry> next;
    };

    ~DX12CommandStorage();

    auto Acquire(ID3D12Device& device, CommandType type) -> std::unique_ptr<Entry>;
    void Return(std::unique_ptr<Entry> entry, bool closed) noexcept;
    void CollectCompleted();
    void Submitted(Entry* entry, ComPtr<ID3D12Fence> fence, std::uint64_t value);
private:
    static bool Completed(const Entry& entry);
    void CollectCompletedLocked();
    void PushReady(std::unique_ptr<Entry> entry);

    std::mutex m_Mutex;
    utils::EnumArray<std::unique_ptr<Entry>, CommandType> m_Ready;
    std::unique_ptr<Entry> m_Pending;
};

class DX12GraphicsCommandList : public GraphicsCommandContext {
public:
    DX12GraphicsCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name, std::shared_ptr<DX12CommandStorage> storage);
    ~DX12GraphicsCommandList() override;
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

    std::shared_ptr<DX12CommandStorage> storage;
    std::unique_ptr<DX12CommandStorage::Entry> storage_entry;
    bool closed = false;

    const RenderPipeline*                  m_Pipeline = nullptr;
    std::unique_ptr<tracy::D3D12ZoneScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyD3D12Ctx                   m_TracyCtx;
    DX12BindlessUtils&              m_BindlessUtils;
};

class DX12ComputeCommandList : public ComputeCommandContext {
public:
    DX12ComputeCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name, std::shared_ptr<DX12CommandStorage> storage);
    ~DX12ComputeCommandList() override;
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final;

    void SetPipeline(const ComputePipeline& pipeline) final;
    void Dispatch(math::vec3u group_count) final;
    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    std::shared_ptr<DX12CommandStorage> storage;
    std::unique_ptr<DX12CommandStorage::Entry> storage_entry;
    bool closed = false;

    const ComputePipeline*                 m_Pipeline = nullptr;
    std::unique_ptr<tracy::D3D12ZoneScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyD3D12Ctx                   m_TracyCtx;
    DX12BindlessUtils&              m_BindlessUtils;
};

class DX12CopyCommandList : public CopyCommandContext {
public:
    DX12CopyCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, TracyD3D12Ctx tracy_context, std::string_view name, std::shared_ptr<DX12CommandStorage> storage);
    ~DX12CopyCommandList() override;
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

    std::shared_ptr<DX12CommandStorage> storage;
    std::unique_ptr<DX12CommandStorage::Entry> storage_entry;
    bool closed = false;

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

DX12CommandStorage::~DX12CommandStorage() {
    // Only returned entries are owned here. Drain iteratively to avoid recursive
    // destruction of the intrusive lists; ready entries need no GPU wait.
    while (m_Pending) {
        auto entry = std::move(m_Pending);
        m_Pending = std::move(entry->next);
        for (const auto& [fence, value] : entry->submissions) {
            if (fence->GetCompletedValue() >= value) continue;
            if (interop::failed(fence->SetEventOnCompletion(value, nullptr)))
                std::terminate();
        }
    }
    magic_enum::enum_for_each<CommandType>([&](CommandType type) {
        while (m_Ready[type]) {
            auto entry = std::move(m_Ready[type]);
            m_Ready[type] = std::move(entry->next);
        }
    });
}

bool DX12CommandStorage::Completed(const Entry& entry) {
    return std::ranges::all_of(entry.submissions, [](const auto& submission) {
        return submission.first->GetCompletedValue() >= submission.second;
    });
}

void DX12CommandStorage::PushReady(std::unique_ptr<Entry> entry) {
    entry->submissions.clear();
    auto& head = m_Ready[entry->type];
    entry->next = std::move(head);
    head = std::move(entry);
}

void DX12CommandStorage::CollectCompletedLocked() {
    // Different queues can complete out of order: do not stop at the first
    // unfinished entry. Never examine entries currently owned by wrappers.
    auto* link = &m_Pending;
    while (*link) {
        if (!Completed(**link)) {
            link = &(*link)->next;
            continue;
        }
        auto entry = std::move(*link);
        *link = std::move(entry->next);
        PushReady(std::move(entry));
    }
}

void DX12CommandStorage::CollectCompleted() {
    std::scoped_lock lock(m_Mutex);
    CollectCompletedLocked();
}

auto DX12CommandStorage::Acquire(ID3D12Device& device, CommandType type) -> std::unique_ptr<Entry> {
    ZoneScopedNS("DX12.NativeCommands.Acquire", 0);
    std::scoped_lock lock(m_Mutex);
    auto& ready = m_Ready[type];
    if (!ready) CollectCompletedLocked();
    if (ready) {
        auto entry = std::move(ready);
        ready = std::move(entry->next);
        if (interop::failed(entry->allocator->Reset()) ||
            interop::failed(entry->list->Reset(entry->allocator.Get(), nullptr)))
            throw std::runtime_error("Failed to reset native command storage");
        return entry;
    }
    auto entry = std::make_unique<Entry>();
    entry->type = type;
    if (interop::failed(device.CreateCommandAllocator(to_d3d_command_type(type), IID_PPV_ARGS(&entry->allocator))) ||
        interop::failed(device.CreateCommandList(0, to_d3d_command_type(type), entry->allocator.Get(), nullptr, IID_PPV_ARGS(&entry->list))))
        throw std::runtime_error("Failed to create native command storage");
    return entry;
}

void DX12CommandStorage::Return(std::unique_ptr<Entry> entry, bool closed) noexcept {
    ZoneScopedNS("DX12.NativeCommands.Return", 0);
    std::scoped_lock lock(m_Mutex);
    if (!closed && interop::failed(entry->list->Close())) {
        // An abandoned, unsubmitted recording is safe to discard on failure.
        if (!entry->submissions.empty()) std::terminate();
        return;
    }
    if (Completed(*entry)) {
        PushReady(std::move(entry));
    } else {
        entry->next = std::move(m_Pending);
        m_Pending = std::move(entry);
    }
}

void DX12CommandStorage::Submitted(Entry* entry, ComPtr<ID3D12Fence> fence, std::uint64_t value) {
    std::scoped_lock lock(m_Mutex);
    for (auto& submission : entry->submissions) {
        if (submission.first.Get() == fence.Get()) {
            submission.second = (std::max)(submission.second, value);
            return;
        }
    }
    entry->submissions.emplace_back(std::move(fence), value);
}

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

DX12GraphicsCommandList::DX12GraphicsCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name, std::shared_ptr<DX12CommandStorage> storage)
    : GraphicsCommandContext(name), storage(std::move(storage)),
      storage_entry(this->storage->Acquire(device, CommandType::Graphics)),
      m_Logger(logger), m_TracyCtx(tracy_context), m_BindlessUtils(bindings) {
    storage_entry->list->SetName(std::wstring(name.begin(), name.end()).c_str());
}

void DX12GraphicsCommandList::Begin() {
    auto& dx12_bindless_utils = m_BindlessUtils;
    auto  descriptor_heaps    = dx12_bindless_utils.GetDescriptorHeaps();
    storage_entry->list->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
    storage_entry->list->SetGraphicsRootSignature(dx12_bindless_utils.GetBindlessRootSignature().Get());

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
        storage_entry->list.Get(),
        true);
#endif

    m_Pipeline = nullptr;
}

void DX12GraphicsCommandList::End() {
    m_TracyZone.reset();
    if (interop::failed(storage_entry->list->Close())) throw std::runtime_error("Failed to close command list");
    closed = true;
}

void DX12GraphicsCommandList::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                              std::span<const GPUBufferBarrier> buffer_barriers,
                                              std::span<const TextureBarrier>   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (interop::failed(storage_entry->list.As(&cmd_list))) {
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

    storage_entry->list->OMSetRenderTargets(
        1, &dx12_render_target.rtv.GetCPUHandle(), false,
        (dx12_depth_stencil && dx12_depth_stencil->dsv) ? &dx12_depth_stencil->dsv.GetCPUHandle() : nullptr);

    if (clear_render_target) {
        if (!render_target_texture.GetDesc().clear_value.has_value()) {
            m_Logger->warn(fmt::format(
                "render target({}) has no clear value but clear render target is requested",
                fmt::styled(render_target_texture.GetName(), fmt::fg(fmt::color::orange))));

        } else {
            const auto clear_color = std::get<ClearColor>(render_target_texture.GetDesc().clear_value.value());
            storage_entry->list->ClearRenderTargetView(dx12_render_target.rtv.GetCPUHandle(), clear_color, 0, nullptr);
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
            storage_entry->list->ClearDepthStencilView(dx12_depth_stencil->dsv.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clear_depth_stencil.depth, clear_depth_stencil.stencil, 0, nullptr);
        }
    }
}

void DX12GraphicsCommandList::EndRendering() {}

void DX12GraphicsCommandList::SetPipeline(const RenderPipeline& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;

    auto& dx12_pipeline = static_cast<const DX12RenderPipeline&>(pipeline);
    storage_entry->list->SetPipelineState(dx12_pipeline.pipeline.Get());
    storage_entry->list->IASetPrimitiveTopology(to_d3d_primitive_topology(dx12_pipeline.GetDesc().assembly_state.primitive));
}

void DX12GraphicsCommandList::SetViewPort(const ViewPort& view_port) {
    const auto d3d_view_port = to_d3d_view_port(view_port);
    storage_entry->list->RSSetViewports(1, &d3d_view_port);
}

void DX12GraphicsCommandList::SetScissorRect(const Rect& scissor_rect) {
    const auto d3d_scissor_rect = to_d3d_rect(scissor_rect);
    storage_entry->list->RSSetScissorRects(1, &d3d_scissor_rect);
}

void DX12GraphicsCommandList::SetBlendColor(const math::Color& color) {
    storage_entry->list->OMSetBlendFactor(color);
}

void DX12GraphicsCommandList::SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset, Format index_format) {
    auto&                   dx12_buffer = static_cast<const DX12GPUBuffer&>(buffer);
    D3D12_INDEX_BUFFER_VIEW ibv{
        .BufferLocation = dx12_buffer.resource->GetGPUVirtualAddress() + offset,
        .SizeInBytes    = static_cast<UINT>(buffer.Size() - offset),
        .Format         = to_dxgi_format(index_format),
    };
    storage_entry->list->IASetIndexBuffer(&ibv);
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
    storage_entry->list->IASetVertexBuffers(start_binding, vbvs.size(), vbvs.data());
}

void DX12GraphicsCommandList::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    storage_entry->list->SetGraphicsRoot32BitConstants(0, sizeof(info) / sizeof(std::uint32_t), &info, 0);
}

void DX12GraphicsCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    storage_entry->list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void DX12GraphicsCommandList::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {
    storage_entry->list->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

void DX12GraphicsCommandList::CopyTextureRegion(const Texture&          src,
                                                math::vec3i             src_offset,
                                                Texture&                dst,
                                                math::vec3i             dst_offset,
                                                math::vec3u             extent,
                                                TextureSubresourceLayer src_layer,
                                                TextureSubresourceLayer dst_layer) {
    copy_texture_region(storage_entry->list, src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

DX12ComputeCommandList::DX12ComputeCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, DX12BindlessUtils& bindings, TracyD3D12Ctx tracy_context, std::string_view name, std::shared_ptr<DX12CommandStorage> storage)
    : ComputeCommandContext(name), storage(std::move(storage)),
      storage_entry(this->storage->Acquire(device, CommandType::Compute)),
      m_Logger(logger), m_TracyCtx(tracy_context), m_BindlessUtils(bindings) {
    storage_entry->list->SetName(std::wstring(name.begin(), name.end()).c_str());
}

void DX12ComputeCommandList::Begin() {
    auto& dx12_bindless_utils = m_BindlessUtils;
    auto  descriptor_heaps    = dx12_bindless_utils.GetDescriptorHeaps();
    storage_entry->list->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
    storage_entry->list->SetComputeRootSignature(dx12_bindless_utils.GetBindlessRootSignature().Get());

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
        storage_entry->list.Get(),
        true);
#endif

    m_Pipeline = nullptr;
}

void DX12ComputeCommandList::End() {
    m_TracyZone.reset();
    if (interop::failed(storage_entry->list->Close())) throw std::runtime_error("Failed to close command list");
    closed = true;
}

void DX12ComputeCommandList::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                             std::span<const GPUBufferBarrier> buffer_barriers,
                                             std::span<const TextureBarrier>   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (interop::failed(storage_entry->list.As(&cmd_list))) {
        const auto error_message = std::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12ComputeCommandList::Dispatch(math::vec3u group_count) {
    if (!group_count.x || !group_count.y || !group_count.z)
        throw std::invalid_argument("Dispatch dimensions must be nonzero");
    storage_entry->list->Dispatch(group_count.x, group_count.y, group_count.z);
}

void DX12ComputeCommandList::SetPipeline(const ComputePipeline& pipeline) {
    if (m_Pipeline == &pipeline) return;
    m_Pipeline = &pipeline;

    auto& dx12_pipeline = static_cast<const DX12ComputePipeline&>(pipeline);
    storage_entry->list->SetPipelineState(dx12_pipeline.pipeline.Get());
}

void DX12ComputeCommandList::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    storage_entry->list->SetComputeRoot32BitConstants(0, sizeof(info) / sizeof(std::uint32_t), &info, 0);
}

DX12CopyCommandList::DX12CopyCommandList(ID3D12Device& device, const std::shared_ptr<spdlog::logger>& logger, TracyD3D12Ctx tracy_context, std::string_view name, std::shared_ptr<DX12CommandStorage> storage)
    : CopyCommandContext(name), storage(std::move(storage)),
      storage_entry(this->storage->Acquire(device, CommandType::Copy)),
      m_Logger(logger), m_TracyCtx(tracy_context) {
    storage_entry->list->SetName(std::wstring(name.begin(), name.end()).c_str());
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
        storage_entry->list.Get(),
        true);
#endif
}

void DX12CopyCommandList::End() {
    m_TracyZone.reset();
    if (interop::failed(storage_entry->list->Close())) throw std::runtime_error("Failed to close command list");
    closed = true;
}

void DX12CopyCommandList::ResourceBarrier(
    std::span<const GlobalBarrier>    global_barriers,
    std::span<const GPUBufferBarrier> buffer_barriers,
    std::span<const TextureBarrier>   texture_barriers) {
    ComPtr<ID3D12GraphicsCommandList7> cmd_list;
    if (interop::failed(storage_entry->list.As(&cmd_list))) {
        const auto error_message = std::format("failed to cast command list to ID3D12GraphicsCommandList7");
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    pipeline_barrier_fn(cmd_list, global_barriers, buffer_barriers, texture_barriers);
}

void DX12CopyCommandList::CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) {
    auto dx12_src = static_cast<const DX12GPUBuffer&>(src).resource.Get();
    auto dx12_dst = static_cast<DX12GPUBuffer&>(dst).resource.Get();

    storage_entry->list->CopyBufferRegion(dx12_dst, dst_offset, dx12_src, src_offset, size);
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

    storage_entry->list->CopyTextureRegion(
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

    storage_entry->list->CopyTextureRegion(
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
    copy_texture_region(storage_entry->list, src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

}  // namespace hitagi::gfx

namespace hitagi::gfx {
DX12GraphicsCommandList::~DX12GraphicsCommandList() {
        ZoneScopedNS("DX12.DestroyGraphicsContext", 0);
        { ZoneScopedNS("DX12.DestroyContext.TracyScope", 0); m_TracyZone.reset(); }
        storage->Return(std::move(storage_entry), closed);
    }
DX12ComputeCommandList::~DX12ComputeCommandList() {
        ZoneScopedNS("DX12.DestroyComputeContext", 0);
        { ZoneScopedNS("DX12.DestroyContext.TracyScope", 0); m_TracyZone.reset(); }
        storage->Return(std::move(storage_entry), closed);
    }
DX12CopyCommandList::~DX12CopyCommandList() {
        ZoneScopedNS("DX12.DestroyCopyContext", 0);
        { ZoneScopedNS("DX12.DestroyContext.TracyScope", 0); m_TracyZone.reset(); }
        storage->Return(std::move(storage_entry), closed);
    }
}
