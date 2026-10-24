module;
#include "interop/tracy_macros.hpp"

export module gfx.metal:command_buffer;
import interop.metal;
import interop.spdlog;
import interop.tracy;

import std;
import math;
import utils;
import gfx.base;
import :types;
import :utils;
import :configs;
import :bindless;
import :resource;

export namespace hitagi::gfx {

// The queue retains a completed recording until its completion event is reached.
// Re-recording or destroying the context cannot invalidate submitted GPU work.
struct MetalCommandRecording {
    ns::SharedPtr<mtl4::CommandAllocator>                          allocator;
    ns::SharedPtr<mtl4::CommandBuffer>                             command_buffer;
    ns::SharedPtr<mtl::ResidencySet>                               residency;
    ns::SharedPtr<mtl4::ArgumentTable>                             argument_table;
    std::pmr::vector<ns::SharedPtr<mtl::Buffer>>                   constants;
    std::pmr::vector<std::shared_ptr<const MetalBindlessSnapshot>> snapshots;
    std::pmr::vector<ns::SharedPtr<mtl::Resource>>                 resources;
    std::pmr::unordered_set<mtl::Resource*>                        resident_resources;
    std::pmr::vector<ns::SharedPtr<ns::Object>>                    objects;
    std::size_t                                                    constant_offset = 0;
    bool                                                           ended           = false;
};

class MetalCommandBuffer {
public:
    explicit MetalCommandBuffer(const ns::SharedPtr<mtl::Device>& device, std::shared_ptr<spdlog::logger> logger);
    virtual ~MetalCommandBuffer() = default;
    auto        GetNativeDevice() const noexcept -> const ns::SharedPtr<mtl::Device>*;
    inline auto GetRecording() const -> std::shared_ptr<const MetalCommandRecording> { return m_Recording; }

protected:
    void BeginRecording();
    void EndRecording();
    void RetainResource(mtl::Resource* resource);
    auto CreateCopyEncoder() -> mtl4::ComputeCommandEncoder*;
    void SetBindlessMetaInfo(const BindlessMetaInfo& info);
    void BindArgumentBuffer(MetalBindlessUtils& bindings);
    void CopyTexture(const Texture& src, math::vec3i src_offset, Texture& dst, math::vec3i dst_offset, math::vec3u extent, TextureSubresourceLayer src_layer, TextureSubresourceLayer dst_layer);

    // Metal has no image-layout transitions. Each encoder starts with a queue-stage
    // barrier; shared events order dependencies between different command queues.
    template <class Encoder>
    void EncoderBarrier(Encoder* encoder) {
        encoder->barrierAfterQueueStages(mtl::StageAll, mtl::StageAll, mtl4::VisibilityOptionDevice);
    }

    const ns::SharedPtr<mtl::Device>&      m_Device;
    std::shared_ptr<spdlog::logger>        m_Logger;
    std::shared_ptr<MetalCommandRecording> m_Recording;
};

class MetalGraphicsCommandBuffer final : public GraphicsCommandContext, public MetalCommandBuffer {
public:
    MetalGraphicsCommandBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, MetalBindlessUtils& bindings, std::string_view name);
    void Begin() final;
    void End() final;
    void ResourceBarrier(std::span<const GlobalBarrier>, std::span<const GPUBufferBarrier>, std::span<const TextureBarrier>) final;
    void BeginRendering(TextureView& render_target, utils::optional_ref<TextureView> depth_stencil, bool clear_render_target, bool clear_depth_stencil) final;
    void EndRendering() final;
    void SetPipeline(const RenderPipeline& pipeline) final;
    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::Color& color) final;
    void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset, Format format) final;
    void SetVertexBuffers(std::uint8_t start, std::span<const std::reference_wrapper<const GPUBuffer>> buffers, std::span<const std::size_t> offsets) final;
    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;
    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) final;
    void CopyTextureRegion(const Texture& src, math::vec3i src_offset, Texture& dst, math::vec3i dst_offset, math::vec3u extent, TextureSubresourceLayer src_layer, TextureSubresourceLayer dst_layer) final;

private:
    MetalBindlessUtils&                       m_BindlessUtils;
    ns::SharedPtr<mtl4::RenderCommandEncoder> m_Encoder;
    const MetalRenderPipeline*                m_Pipeline    = nullptr;
    const MetalBuffer*                        m_IndexBuffer = nullptr;
    std::size_t                               m_IndexOffset = 0;
    mtl::IndexType                            m_IndexType   = mtl::IndexTypeUInt32;
    mtl::PrimitiveType                        m_Primitive   = mtl::PrimitiveTypeTriangle;
};

class MetalComputeCommandBuffer final : public ComputeCommandContext, public MetalCommandBuffer {
public:
    MetalComputeCommandBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, MetalBindlessUtils& bindings, std::string_view name);
    void Begin() final;
    void End() final;
    void ResourceBarrier(std::span<const GlobalBarrier>, std::span<const GPUBufferBarrier>, std::span<const TextureBarrier>) final;
    void SetPipeline(const ComputePipeline& pipeline) final;
    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    inline auto GetMetalEncoder() const noexcept { return m_Encoder.get(); }

private:
    MetalBindlessUtils&                        m_BindlessUtils;
    ns::SharedPtr<mtl4::ComputeCommandEncoder> m_Encoder;
};

class MetalTransferCommandBuffer final : public CopyCommandContext, public MetalCommandBuffer {
public:
    MetalTransferCommandBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, std::string_view name);
    void Begin() final;
    void End() final;
    void ResourceBarrier(std::span<const GlobalBarrier>, std::span<const GPUBufferBarrier>, std::span<const TextureBarrier>) final;
    void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) final;
    void CopyBufferToTexture(const GPUBuffer& src, std::size_t src_offset, Texture& dst, math::vec3i dst_offset, math::vec3u extent, TextureSubresourceLayer layer) final;
    void CopyTextureToBuffer(const Texture& src, math::vec3i src_offset, math::vec3u extent, GPUBuffer& dst, std::size_t dst_offset, TextureSubresourceLayer layer) final;
    void CopyTextureRegion(const Texture& src, math::vec3i src_offset, Texture& dst, math::vec3i dst_offset, math::vec3u extent, TextureSubresourceLayer src_layer, TextureSubresourceLayer dst_layer) final;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MetalCommandBuffer::MetalCommandBuffer(const ns::SharedPtr<mtl::Device>& device, std::shared_ptr<spdlog::logger> logger) : m_Device(device), m_Logger(std::move(logger)) {}

auto MetalCommandBuffer::GetNativeDevice() const noexcept -> const ns::SharedPtr<mtl::Device>* { return &m_Device; }

void MetalCommandBuffer::RetainResource(mtl::Resource* resource) {
    if (m_Recording->resident_resources.insert(resource).second) {
        m_Recording->residency->addAllocation(resource);
        m_Recording->resources.emplace_back(ns::RetainPtr(resource));
    }
}

auto MetalCommandBuffer::CreateCopyEncoder() -> mtl4::ComputeCommandEncoder* {
    auto encoder = m_Recording->command_buffer->computeCommandEncoder();
    check_metal(m_Logger, encoder != nullptr, "Create Metal copy encoder failed");
    EncoderBarrier(encoder);
    return encoder;
}

void MetalCommandBuffer::EndRecording() {
    m_Recording->residency->commit();
    m_Recording->command_buffer->useResidencySet(m_Recording->residency.get());
    m_Recording->command_buffer->endCommandBuffer();
    m_Recording->ended = true;
}

void MetalCommandBuffer::SetBindlessMetaInfo(const BindlessMetaInfo& info) {
    // Immutable per-draw slices, instead of one native allocation per draw.
    constexpr std::size_t chunk_size   = 64 * 1024;
    constexpr std::size_t alignment    = 256;
    const auto            offset       = m_Recording->constant_offset;
    const auto            chunk        = offset / chunk_size;
    const auto            within_chunk = offset % chunk_size;
    if (chunk == m_Recording->constants.size()) {
        auto buffer = ns::TransferPtr(m_Device->newBuffer(chunk_size, mtl::ResourceStorageModeShared));
        check_metal(m_Logger, buffer.get() != nullptr, "Create Metal metadata upload buffer failed");
        m_Recording->constants.push_back(std::move(buffer));
    }
    auto& buffer = m_Recording->constants[chunk];
    std::memcpy(static_cast<std::byte*>(buffer->contents()) + within_chunk, &info, sizeof(info));
    RetainResource(buffer.get());
    m_Recording->argument_table->setAddress(buffer->gpuAddress() + within_chunk, metal_bindless_meta_slot);
    static_assert(sizeof(info) <= alignment);
    m_Recording->constant_offset += alignment;
}

void MetalCommandBuffer::BeginRecording() {
    if (!m_Recording || m_Recording.use_count() != 1) {
        m_Recording                 = std::allocate_shared<MetalCommandRecording>(std::pmr::polymorphic_allocator<MetalCommandRecording>{});
        m_Recording->allocator      = ns::TransferPtr(m_Device->newCommandAllocator());
        m_Recording->command_buffer = ns::TransferPtr(m_Device->newCommandBuffer());
        check_metal(m_Logger, m_Recording->allocator.get() != nullptr, "Create Metal command allocator failed");
        check_metal(m_Logger, m_Recording->command_buffer.get() != nullptr, "Create Metal command buffer failed");
        ns::Error* error          = nullptr;
        auto       residency_desc = ns::TransferPtr(mtl::ResidencySetDescriptor::alloc()->init());
        m_Recording->residency    = ns::TransferPtr(m_Device->newResidencySet(residency_desc.get(), &error));
        check_metal(m_Logger, m_Recording->residency.get() != nullptr, "Create Metal residency set failed", error);
        auto table_desc = ns::TransferPtr(mtl4::ArgumentTableDescriptor::alloc()->init());
        table_desc->setMaxBufferBindCount(metal_max_buffer_bindings);
        table_desc->setInitializeBindings(true);
        m_Recording->argument_table = ns::TransferPtr(m_Device->newArgumentTable(table_desc.get(), &error));
        check_metal(m_Logger, m_Recording->argument_table.get() != nullptr, "Create Metal argument table failed", error);
    } else {
        m_Recording->allocator->reset();
        m_Recording->residency->removeAllAllocations();
        m_Recording->snapshots.clear();
        m_Recording->resources.clear();
        m_Recording->resident_resources.clear();
        m_Recording->objects.clear();
    }
    m_Recording->constant_offset = 0;
    m_Recording->ended           = false;
    m_Recording->command_buffer->beginCommandBuffer(m_Recording->allocator.get());
}

void MetalCommandBuffer::BindArgumentBuffer(MetalBindlessUtils& bindings) {
    auto snapshot = bindings.GetSnapshot();
    if (!m_Recording->snapshots.empty() && m_Recording->snapshots.back() == snapshot) return;
    for (const auto& resource : snapshot->resources) RetainResource(resource.get());
    RetainResource(snapshot->buffer.get());
    m_Recording->argument_table->setAddress(snapshot->buffer->gpuAddress(), metal_argument_buffer_slot);
    m_Recording->snapshots.push_back(std::move(snapshot));
}

void MetalCommandBuffer::CopyTexture(
    const Texture&          src,
    math::vec3i             src_offset,
    Texture&                dst,
    math::vec3i             dst_offset,
    math::vec3u             extent,
    TextureSubresourceLayer src_layer,
    TextureSubresourceLayer dst_layer) {
    auto  encoder = CreateCopyEncoder();
    auto& source  = dynamic_cast<const MetalTexture&>(src);
    auto& target  = dynamic_cast<MetalTexture&>(dst);
    RetainResource(source.native.get());
    RetainResource(target.native.get());
    for (unsigned layer = 0; layer < src_layer.layer_count; ++layer)
        encoder->copyFromTexture(
            source.native.get(),
            src_layer.base_array_layer + layer,
            src_layer.mip_level,
            mtl::Origin{
                static_cast<ns::UInteger>(src_offset.x),
                static_cast<ns::UInteger>(src_offset.y),
                static_cast<ns::UInteger>(src_offset.z),
            },
            mtl::Size{
                extent.x,
                extent.y,
                extent.z,
            },
            target.native.get(),
            dst_layer.base_array_layer + layer,
            dst_layer.mip_level,
            mtl::Origin{
                static_cast<ns::UInteger>(dst_offset.x),
                static_cast<ns::UInteger>(dst_offset.y),
                static_cast<ns::UInteger>(dst_offset.z),
            });
    encoder->endEncoding();
}

MetalGraphicsCommandBuffer::MetalGraphicsCommandBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, MetalBindlessUtils& bindings, std::string_view name)
    : GraphicsCommandContext(name), MetalCommandBuffer(device, logger), m_BindlessUtils(bindings) {}

void MetalGraphicsCommandBuffer::Begin() {
    ZoneScoped;
    BeginRecording();
    m_Pipeline    = nullptr;
    m_IndexBuffer = nullptr;
}

void MetalGraphicsCommandBuffer::End() {
    ZoneScoped;
    if (m_Encoder) EndRendering();
    EndRecording();
}

void MetalGraphicsCommandBuffer::ResourceBarrier(std::span<const GlobalBarrier> global, std::span<const GPUBufferBarrier> buffers, std::span<const TextureBarrier> textures) {
    if (m_Encoder && (!global.empty() || !buffers.empty() || !textures.empty())) EncoderBarrier(m_Encoder.get());
}

void MetalGraphicsCommandBuffer::BeginRendering(TextureView& render_target, utils::optional_ref<TextureView> depth_stencil, bool clear_render_target, bool clear_depth_stencil) {
    if (m_Encoder) throw std::logic_error("Metal render pass is already active");
    auto  pass = ns::TransferPtr(mtl4::RenderPassDescriptor::alloc()->init());
    auto& view = dynamic_cast<MetalTextureView&>(render_target);
    RetainResource(view.native.get());
    auto* attachment = pass->colorAttachments()->object(0);
    attachment->setTexture(view.native.get());
    attachment->setLoadAction(clear_render_target ? mtl::LoadActionClear : mtl::LoadActionLoad);
    attachment->setStoreAction(mtl::StoreActionStore);
    auto color = math::Color(0, 0, 0, 0);
    if (auto clear_value = render_target.GetDesc().texture->GetDesc().clear_value; clear_value && std::holds_alternative<ClearColor>(*clear_value)) {
        color = std::get<ClearColor>(*clear_value);
    }
    attachment->setClearColor(mtl::ClearColor{color.r, color.g, color.b, color.a});
    if (depth_stencil) {
        auto& depth_view = dynamic_cast<MetalTextureView&>(depth_stencil->get());
        RetainResource(depth_view.native.get());
        auto* depth_attachment = pass->depthAttachment();
        depth_attachment->setTexture(depth_view.native.get());
        depth_attachment->setLoadAction(clear_depth_stencil ? mtl::LoadActionClear : mtl::LoadActionLoad);
        depth_attachment->setStoreAction(mtl::StoreActionStore);
        float value = 1;
        if (auto clear_value = depth_view.GetDesc().texture->GetDesc().clear_value; clear_value && std::holds_alternative<ClearDepthStencil>(*clear_value)) {
            value = std::get<ClearDepthStencil>(*clear_value).depth;
        }
        depth_attachment->setClearDepth(value);
    }
    m_Encoder = ns::RetainPtr(m_Recording->command_buffer->renderCommandEncoder(pass.get()));
    check_metal(m_Logger, m_Encoder.get() != nullptr, "Create Metal render encoder");
    EncoderBarrier(m_Encoder.get());
    m_Encoder->setArgumentTable(m_Recording->argument_table.get(), mtl::RenderStageVertex | mtl::RenderStageFragment);
}

void MetalGraphicsCommandBuffer::EndRendering() {
    m_Encoder->endEncoding();
    m_Encoder.reset();
}

void MetalGraphicsCommandBuffer::SetPipeline(const RenderPipeline& pipeline) {
    m_Pipeline = &dynamic_cast<const MetalRenderPipeline&>(pipeline);
    m_Recording->objects.emplace_back(ns::RetainPtr(static_cast<ns::Object*>(m_Pipeline->native.get())));
    if (m_Pipeline->depth) m_Recording->objects.emplace_back(ns::RetainPtr(static_cast<ns::Object*>(m_Pipeline->depth.get())));
    m_Encoder->setRenderPipelineState(m_Pipeline->native.get());
    m_Encoder->setDepthStencilState(m_Pipeline->depth.get());
    const auto& rasterization_state = pipeline.GetDesc().rasterization_state;
    m_Encoder->setCullMode(
        rasterization_state.cull_mode == CullMode::Back    ? mtl::CullModeBack
        : rasterization_state.cull_mode == CullMode::Front ? mtl::CullModeFront
                                                           : mtl::CullModeNone);
    m_Encoder->setFrontFacingWinding(rasterization_state.front_counter_clockwise ? mtl::WindingCounterClockwise : mtl::WindingClockwise);
    m_Encoder->setTriangleFillMode(rasterization_state.fill_mode == FillMode::Wireframe ? mtl::TriangleFillModeLines : mtl::TriangleFillModeFill);
    m_Encoder->setDepthClipMode(rasterization_state.depth_clamp_enable ? mtl::DepthClipModeClamp : mtl::DepthClipModeClip);
    if (rasterization_state.depth_bias_enable) {
        m_Encoder->setDepthBias(rasterization_state.depth_bias, rasterization_state.depth_bias_slope_factor, rasterization_state.depth_bias_clamp);
    }
    switch (pipeline.GetDesc().assembly_state.primitive) {
        case PrimitiveTopology::PointList:
            m_Primitive = mtl::PrimitiveTypePoint;
            break;
        case PrimitiveTopology::LineList:
            m_Primitive = mtl::PrimitiveTypeLine;
            break;
        case PrimitiveTopology::LineStrip:
            m_Primitive = mtl::PrimitiveTypeLineStrip;
            break;
        case PrimitiveTopology::TriangleStrip:
            m_Primitive = mtl::PrimitiveTypeTriangleStrip;
            break;
        default:
            m_Primitive = mtl::PrimitiveTypeTriangle;
            break;
    }
}

void MetalGraphicsCommandBuffer::SetViewPort(const ViewPort& view_port) {
    m_Encoder->setViewport(mtl::Viewport{
        .originX = view_port.x,
        .originY = view_port.y,
        .width   = view_port.width,
        .height  = view_port.height,
        .znear   = view_port.min_depth,
        .zfar    = view_port.max_depth,
    });
}

void MetalGraphicsCommandBuffer::SetScissorRect(const Rect& scissor_rect) {
    m_Encoder->setScissorRect(mtl::ScissorRect{
        .x      = scissor_rect.x,
        .y      = scissor_rect.y,
        .width  = scissor_rect.width,
        .height = scissor_rect.height,
    });
}

void MetalGraphicsCommandBuffer::SetBlendColor(const math::Color& color) { m_Encoder->setBlendColor(color.r, color.g, color.b, color.a); }

void MetalGraphicsCommandBuffer::SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset, Format format) {
    m_IndexBuffer = &dynamic_cast<const MetalBuffer&>(buffer);
    RetainResource(m_IndexBuffer->native.get());
    m_IndexOffset = offset;
    m_IndexType   = format == Format::R16_UINT ? mtl::IndexTypeUInt16 : mtl::IndexTypeUInt32;
}

void MetalGraphicsCommandBuffer::SetVertexBuffers(std::uint8_t start, std::span<const std::reference_wrapper<const GPUBuffer>> buffers, std::span<const std::size_t> offsets) {
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        auto* native = dynamic_cast<const MetalBuffer&>(buffers[i].get()).native.get();
        RetainResource(native);
        m_Recording->argument_table->setAddress(native->gpuAddress() + (offsets.empty() ? 0 : offsets[i]), start + i + metal_vertex_buffer_slot);
    }
}

void MetalGraphicsCommandBuffer::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    BindArgumentBuffer(m_BindlessUtils);
    SetBindlessMetaInfo(info);
    m_Encoder->setArgumentTable(m_Recording->argument_table.get(), mtl::RenderStageVertex | mtl::RenderStageFragment);
}

void MetalGraphicsCommandBuffer::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    m_Encoder->drawPrimitives(m_Primitive, first_vertex, vertex_count, instance_count, first_instance);
}

void MetalGraphicsCommandBuffer::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {
    check_metal(m_Logger, m_IndexBuffer != nullptr, "Metal index buffer is not bound");
    m_Encoder->drawIndexedPrimitives(
        m_Primitive,
        index_count,
        m_IndexType,
        m_IndexBuffer->native->gpuAddress() + m_IndexOffset + first_index * (m_IndexType == mtl::IndexTypeUInt16 ? 2 : 4),
        m_IndexBuffer->Size() - m_IndexOffset - first_index * (m_IndexType == mtl::IndexTypeUInt16 ? 2 : 4),
        instance_count, static_cast<std::int32_t>(base_vertex),
        first_instance);
}

void MetalGraphicsCommandBuffer::CopyTextureRegion(
    const Texture&          src,
    math::vec3i             src_offset,
    Texture&                dst,
    math::vec3i             dst_offset,
    math::vec3u             extent,
    TextureSubresourceLayer src_layer,
    TextureSubresourceLayer dst_layer) {
    if (m_Encoder) throw std::logic_error("Metal texture copy requires ending the render pass");
    CopyTexture(src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

MetalComputeCommandBuffer::MetalComputeCommandBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, MetalBindlessUtils& bindings, std::string_view name)
    : ComputeCommandContext(name), MetalCommandBuffer(device, logger), m_BindlessUtils(bindings) {}

void MetalComputeCommandBuffer::Begin() {
    ZoneScoped;
    BeginRecording();
    m_Encoder = ns::RetainPtr(m_Recording->command_buffer->computeCommandEncoder());
    check_metal(m_Logger, bool(m_Encoder), "Create Metal compute encoder failed");
    EncoderBarrier(m_Encoder.get());
}

void MetalComputeCommandBuffer::End() {
    ZoneScoped;
    m_Encoder->endEncoding();
    m_Encoder.reset();
    EndRecording();
}

void MetalComputeCommandBuffer::ResourceBarrier(std::span<const GlobalBarrier> global, std::span<const GPUBufferBarrier> buffers, std::span<const TextureBarrier> textures) {
    if (m_Encoder && (!global.empty() || !buffers.empty() || !textures.empty())) EncoderBarrier(m_Encoder.get());
}

void MetalComputeCommandBuffer::SetPipeline(const ComputePipeline& pipeline) {
    auto* native = static_cast<const MetalComputePipeline&>(pipeline).native.get();
    m_Recording->objects.emplace_back(ns::RetainPtr(static_cast<ns::Object*>(native)));
    m_Encoder->setComputePipelineState(native);
}

void MetalComputeCommandBuffer::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    BindArgumentBuffer(m_BindlessUtils);
    SetBindlessMetaInfo(info);
    m_Encoder->setArgumentTable(m_Recording->argument_table.get());
}

MetalTransferCommandBuffer::MetalTransferCommandBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, std::string_view name) : CopyCommandContext(name), MetalCommandBuffer(device, logger) {}

void MetalTransferCommandBuffer::Begin() {
    ZoneScoped;
    BeginRecording();
}

void MetalTransferCommandBuffer::End() {
    ZoneScoped;
    EndRecording();
}

void MetalTransferCommandBuffer::ResourceBarrier(std::span<const GlobalBarrier>, std::span<const GPUBufferBarrier>, std::span<const TextureBarrier>) {}

void MetalTransferCommandBuffer::CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) {
    RetainResource(dynamic_cast<const MetalBuffer&>(src).native.get());
    RetainResource(dynamic_cast<MetalBuffer&>(dst).native.get());
    auto encoder = CreateCopyEncoder();
    encoder->copyFromBuffer(dynamic_cast<const MetalBuffer&>(src).native.get(), src_offset, dynamic_cast<MetalBuffer&>(dst).native.get(), dst_offset, size);
    encoder->endEncoding();
}

void MetalTransferCommandBuffer::CopyBufferToTexture(const GPUBuffer& src, std::size_t src_offset, Texture& dst, math::vec3i dst_offset, math::vec3u extent, TextureSubresourceLayer layer) {
    RetainResource(dynamic_cast<const MetalBuffer&>(src).native.get());
    RetainResource(dynamic_cast<MetalTexture&>(dst).native.get());
    auto encoder = CreateCopyEncoder();
    auto row     = extent.x * get_format_byte_size(dst.GetDesc().format);
    for (unsigned i = 0; i < layer.layer_count; ++i)
        encoder->copyFromBuffer(
            dynamic_cast<const MetalBuffer&>(src).native.get(),
            src_offset + i * row * extent.y * extent.z,
            row,
            row * extent.y,
            mtl::Size{extent.x, extent.y, extent.z},
            dynamic_cast<MetalTexture&>(dst).native.get(),
            layer.base_array_layer + i, layer.mip_level,
            mtl::Origin{static_cast<ns::UInteger>(dst_offset.x), static_cast<ns::UInteger>(dst_offset.y), static_cast<ns::UInteger>(dst_offset.z)});
    encoder->endEncoding();
}

void MetalTransferCommandBuffer::CopyTextureToBuffer(const Texture& src, math::vec3i src_offset, math::vec3u extent, GPUBuffer& dst, std::size_t dst_offset, TextureSubresourceLayer layer) {
    RetainResource(dynamic_cast<const MetalTexture&>(src).native.get());
    RetainResource(dynamic_cast<MetalBuffer&>(dst).native.get());
    auto encoder = CreateCopyEncoder();
    auto row     = extent.x * get_format_byte_size(src.GetDesc().format);
    for (unsigned i = 0; i < layer.layer_count; ++i)
        encoder->copyFromTexture(
            dynamic_cast<const MetalTexture&>(src).native.get(),
            layer.base_array_layer + i,
            layer.mip_level,
            mtl::Origin{static_cast<ns::UInteger>(src_offset.x), static_cast<ns::UInteger>(src_offset.y), static_cast<ns::UInteger>(src_offset.z)},
            mtl::Size{extent.x, extent.y, extent.z},
            dynamic_cast<MetalBuffer&>(dst).native.get(),
            dst_offset + i * row * extent.y * extent.z, row,
            row * extent.y);
    encoder->endEncoding();
}

void MetalTransferCommandBuffer::CopyTextureRegion(
    const Texture&          src,
    math::vec3i             src_offset,
    Texture&                dst,
    math::vec3i             dst_offset,
    math::vec3u             extent,
    TextureSubresourceLayer src_layer,
    TextureSubresourceLayer dst_layer) {
    CopyTexture(src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

}  // namespace hitagi::gfx
