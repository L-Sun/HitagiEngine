module;
#include <vulkan/vulkan_raii.hpp>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#include <cstring>
#include <spdlog/logger.h>
#include <fmt/color.h>

export module gfx.vulkan:command_buffer;
import std;
import core;
import utils;
import math;
import gfx.base;
import magic_enum;
import :types;
import :bindless;
import :utils;
import :resource;
import :configs;

export namespace hitagi::gfx {

class VulkanGraphicsCommandBuffer final : public GraphicsCommandContext {
public:
    VulkanGraphicsCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, const std::shared_ptr<spdlog::logger>& logger, VulkanBindlessUtils& bindings, TracyVkCtx tracy_context, std::string_view name);

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
    void SetVertexBuffers(std::uint8_t                                             start_binding,
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

    void BlitTexture(const Texture&          src,
                     math::vec3i             src_offset,
                     math::vec3u             src_extent,
                     Texture&                dst,
                     math::vec3i             dst_offset,
                     math::vec3u             dst_extent,
                     TextureSubresourceLayer src_layer = {},
                     TextureSubresourceLayer dst_layer = {});

    vk::raii::CommandBuffer                                command_buffer;
    std::shared_ptr<vk::raii::Semaphore>                   swap_chain_image_available_semaphore;
    std::pmr::vector<std::shared_ptr<vk::raii::Semaphore>> swap_chain_presentable_semaphores;

private:
    const VulkanRenderPipeline*        m_Pipeline = nullptr;
    std::unique_ptr<tracy::VkCtxScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyVkCtx                      m_TracyCtx;
    VulkanBindlessUtils&            m_BindlessUtils;
};

class VulkanComputeCommandBuffer final : public ComputeCommandContext {
public:
    VulkanComputeCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, const std::shared_ptr<spdlog::logger>& logger, VulkanBindlessUtils& bindings, TracyVkCtx tracy_context, std::string_view name);

    void Begin() final;
    void End() final;

    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final;

    void SetPipeline(const ComputePipeline& pipeline) final;

    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    vk::raii::CommandBuffer command_buffer;

private:
    const VulkanComputePipeline*       m_Pipeline = nullptr;
    std::unique_ptr<tracy::VkCtxScope> m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyVkCtx                      m_TracyCtx;
    VulkanBindlessUtils&            m_BindlessUtils;
};

class VulkanTransferCommandBuffer final : public CopyCommandContext {
public:
    VulkanTransferCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, const std::shared_ptr<spdlog::logger>& logger, TracyVkCtx tracy_context, std::string_view name);

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

    vk::raii::CommandBuffer command_buffer;

    std::shared_ptr<vk::raii::Semaphore>                   swap_chain_image_available_semaphore;
    std::pmr::vector<std::shared_ptr<vk::raii::Semaphore>> swap_chain_presentable_semaphores;
    std::unique_ptr<tracy::VkCtxScope>                     m_TracyZone;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    TracyVkCtx                      m_TracyCtx;
};

inline auto to_vk_buffer_barrier(const GPUBufferBarrier& barrier) -> vk::BufferMemoryBarrier2 {
    const auto& vk_buffer = dynamic_cast<VulkanBuffer&>(barrier.buffer);
    return {
        .srcStageMask  = to_vk_pipeline_stage2(barrier.src_stage),
        .srcAccessMask = to_vk_access_flags(barrier.src_access),
        .dstStageMask  = to_vk_pipeline_stage2(barrier.dst_stage),
        .dstAccessMask = to_vk_access_flags(barrier.dst_access),
        .buffer        = **vk_buffer.buffer,
        .offset        = 0,
        .size          = vk_buffer.Size(),
    };
}

inline auto to_vk_image_barrier(const TextureBarrier& barrier) -> vk::ImageMemoryBarrier2 {
    const auto vk_image = dynamic_cast<VulkanImage&>(barrier.texture).image_handle;

    return {
        .srcStageMask     = to_vk_pipeline_stage2(barrier.src_stage),
        .srcAccessMask    = to_vk_access_flags(barrier.src_access),
        .dstStageMask     = to_vk_pipeline_stage2(barrier.dst_stage),
        .dstAccessMask    = to_vk_access_flags(barrier.dst_access),
        .oldLayout        = to_vk_image_layout(barrier.src_layout),
        .newLayout        = to_vk_image_layout(barrier.dst_layout),
        .image            = vk_image,
        .subresourceRange = {
            .aspectMask     = get_vk_image_aspect(barrier.texture.GetDesc()),
            .baseMipLevel   = 0,
            .levelCount     = barrier.texture.GetDesc().mip_levels,
            .baseArrayLayer = 0,
            .layerCount     = barrier.texture.GetDesc().array_size,
        }};
}

}  // namespace hitagi::gfx

namespace hitagi::gfx {

inline void pipeline_barrier_fn(vk::raii::CommandBuffer&          command_buffer,
                                std::span<const GlobalBarrier>    global_barriers,
                                std::span<const GPUBufferBarrier> buffer_barriers,
                                std::span<const TextureBarrier>   texture_barriers) {
    // convert to vulkan barriers
    std::pmr::vector<vk::MemoryBarrier2>       vk_memory_barriers;
    std::pmr::vector<vk::BufferMemoryBarrier2> vk_buffer_barriers;
    std::pmr::vector<vk::ImageMemoryBarrier2>  vk_image_barriers;

    std::transform(global_barriers.begin(), global_barriers.end(), std::back_inserter(vk_memory_barriers), to_vk_memory_barrier);
    std::transform(buffer_barriers.begin(), buffer_barriers.end(), std::back_inserter(vk_buffer_barriers), to_vk_buffer_barrier);
    std::transform(texture_barriers.begin(), texture_barriers.end(), std::back_inserter(vk_image_barriers), to_vk_image_barrier);

    command_buffer.pipelineBarrier2(vk::DependencyInfo{
        .memoryBarrierCount       = static_cast<std::uint32_t>(vk_memory_barriers.size()),
        .pMemoryBarriers          = vk_memory_barriers.data(),
        .bufferMemoryBarrierCount = static_cast<std::uint32_t>(vk_buffer_barriers.size()),
        .pBufferMemoryBarriers    = vk_buffer_barriers.data(),
        .imageMemoryBarrierCount  = static_cast<std::uint32_t>(vk_image_barriers.size()),
        .pImageMemoryBarriers     = vk_image_barriers.data(),
    });
}

inline auto create_command_buffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, std::string_view name) -> vk::raii::CommandBuffer {
    vk::raii::CommandBuffer command_buffer = std::move(vk::raii::CommandBuffers(
                                                           device,
                                                           {
                                                               .commandPool        = *pool,
                                                               .level              = vk::CommandBufferLevel::ePrimary,
                                                               .commandBufferCount = 1,
                                                           })
                                                           .front());
    create_vk_debug_object_info(command_buffer, name, device);
    return command_buffer;
}

inline void copy_texture_region(const vk::raii::CommandBuffer& command_buffer,
                                const Texture&                 src,
                                math::vec3i                    src_offset,
                                Texture&                       dst,
                                math::vec3i                    dst_offset,
                                math::vec3u                    extent,
                                TextureSubresourceLayer        src_layer,
                                TextureSubresourceLayer        dst_layer) {
    auto& vk_src_image = static_cast<const VulkanImage&>(src);
    auto& vk_dst_image = static_cast<const VulkanImage&>(dst);

    const vk::ImageCopy copy_region = {
        .srcSubresource = to_vk_image_subresource_layer(src_layer, src.GetDesc()),
        .srcOffset      = to_vk_offset3D(src_offset),
        .dstSubresource = to_vk_image_subresource_layer(dst_layer, dst.GetDesc()),
        .dstOffset      = to_vk_offset3D(dst_offset),
        .extent         = to_vk_extent3D(extent),
    };

    command_buffer.copyImage(
        vk_src_image.image_handle,
        to_vk_image_layout(src.GetCurrentLayout()),
        vk_dst_image.image_handle,
        to_vk_image_layout(dst.GetCurrentLayout()),
        copy_region);
}

VulkanGraphicsCommandBuffer::VulkanGraphicsCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, const std::shared_ptr<spdlog::logger>& logger, VulkanBindlessUtils& bindings, TracyVkCtx tracy_context, std::string_view name)
    : GraphicsCommandContext(name), m_Logger(logger), m_TracyCtx(tracy_context), m_BindlessUtils(bindings), command_buffer(create_command_buffer(device, pool, name)) {}

void VulkanGraphicsCommandBuffer::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                                  std::span<const GPUBufferBarrier> buffer_barriers,
                                                  std::span<const TextureBarrier>   texture_barriers) {
    pipeline_barrier_fn(command_buffer, global_barriers, buffer_barriers, texture_barriers);

    // workaround for swapchain semaphore
    for (auto& texture_barrier : texture_barriers) {
        auto& vk_image = static_cast<VulkanImage&>(texture_barrier.texture);

        if (vk_image.swap_chain == nullptr) continue;

        if (texture_barrier.dst_layout == TextureLayout::Present) {
            swap_chain_presentable_semaphores.emplace_back(vk_image.swap_chain->GetSemaphores().presentable);
        } else if (texture_barrier.dst_layout == TextureLayout::RenderTarget ||
                   texture_barrier.dst_layout == TextureLayout::CopyDst) {
            swap_chain_image_available_semaphore = vk_image.swap_chain->GetSemaphores().image_available;
        }
    }
}

void VulkanGraphicsCommandBuffer::Begin() {
    command_buffer.begin({
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    auto& vk_bindless_utils = m_BindlessUtils;

    vk_bindless_utils.Bind(command_buffer);

#ifdef TRACY_ENABLE
    m_TracyZone = std::make_unique<tracy::VkCtxScope>(
        m_TracyCtx,
        __LINE__,
        __FILE__,
        sizeof(__FILE__) - 1,
        __FUNCTION__,
        std::strlen(__FUNCTION__),
        m_Name.data(),
        m_Name.size(),
        *command_buffer,
        true);
#endif
}

void VulkanGraphicsCommandBuffer::End() {
    m_TracyZone.reset();
    TracyVkCollect(m_TracyCtx, *command_buffer);
    command_buffer.end();
}

void VulkanGraphicsCommandBuffer::BeginRendering(TextureView& render_target, utils::optional_ref<TextureView> depth_stencil, bool clear_render_target, bool clear_depth_stencil) {
    auto& color_attachment_texture     = *render_target.GetDesc().texture;
    auto& vk_color_attachment_view     = static_cast<VulkanTextureView&>(render_target);
    auto& vk_color_attachment_image    = static_cast<VulkanImage&>(color_attachment_texture);

    vk::RenderingAttachmentInfo color_attachment, depth_attachment, stencil_attachment;

    color_attachment = {
        .imageView   = *vk_color_attachment_view.image_view.value(),
        .imageLayout = to_vk_image_layout(color_attachment_texture.GetCurrentLayout()),
        .loadOp      = clear_render_target && color_attachment_texture.GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = clear_render_target && color_attachment_texture.GetDesc().clear_value
                           ? to_vk_clear_value(color_attachment_texture.GetDesc().clear_value.value())
                           : vk::ClearValue{},
    };
    if (depth_stencil.has_value()) {
        auto& depth_stencil_texture = *depth_stencil->get().GetDesc().texture;
        auto& vk_depth_stencil_view = static_cast<VulkanTextureView&>(depth_stencil->get());
        depth_attachment            = {
            .imageView   = *vk_depth_stencil_view.image_view.value(),
            .imageLayout = to_vk_image_layout(depth_stencil_texture.GetCurrentLayout()),
            .loadOp      = clear_depth_stencil && depth_stencil_texture.GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = clear_depth_stencil && depth_stencil_texture.GetDesc().clear_value
                               ? to_vk_clear_value(depth_stencil_texture.GetDesc().clear_value.value())
                               : vk::ClearValue{},
        };
        switch (depth_stencil_texture.GetDesc().format) {
            case Format::D24_UNORM_S8_UINT:
            case Format::D32_FLOAT_S8X24_UINT: {
                stencil_attachment = {
                    .imageView   = *vk_depth_stencil_view.image_view.value(),
                    .imageLayout = to_vk_image_layout(depth_stencil_texture.GetCurrentLayout()),
                    .loadOp      = clear_depth_stencil && depth_stencil_texture.GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                    .storeOp     = vk::AttachmentStoreOp::eStore,
                    .clearValue  = clear_depth_stencil && depth_stencil_texture.GetDesc().clear_value
                                       ? to_vk_clear_value(depth_stencil_texture.GetDesc().clear_value.value())
                                       : vk::ClearValue{},
                };
            }
            default:
                break;
        }
    }

    command_buffer.beginRendering({
        .renderArea = {
            .offset = {.x = 0, .y = 0},
            .extent = {
                .width  = vk_color_attachment_image.GetDesc().width,
                .height = vk_color_attachment_image.GetDesc().height,
            },
        },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
        .pDepthAttachment     = &depth_attachment,
        .pStencilAttachment   = &stencil_attachment,
    });
};

void VulkanGraphicsCommandBuffer::EndRendering() {
    command_buffer.endRendering();
}

void VulkanGraphicsCommandBuffer::SetPipeline(const RenderPipeline& pipeline) {
    auto vk_pipeline = &static_cast<const VulkanRenderPipeline&>(pipeline);
    if (m_Pipeline == vk_pipeline) return;
    m_Pipeline = vk_pipeline;
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, **m_Pipeline->pipeline);
}

void VulkanGraphicsCommandBuffer::SetViewPort(const ViewPort& view_port) {
    command_buffer.setViewport(0, to_vk_viewport(view_port));
}

void VulkanGraphicsCommandBuffer::SetScissorRect(const Rect& scissor_rect) {
    command_buffer.setScissor(
        0,
        vk::Rect2D{
            .offset = {.x = static_cast<std::int32_t>(scissor_rect.x), .y = static_cast<std::int32_t>(scissor_rect.y)},
            .extent = {.width = scissor_rect.width, .height = scissor_rect.height},
        });
}

void VulkanGraphicsCommandBuffer::SetBlendColor(const math::Color& color) {
    command_buffer.setBlendConstants(color.data);
}

void VulkanGraphicsCommandBuffer::SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset, Format index_format) {
    vk::IndexType index_type;
    if (index_format == Format::R32_UINT) {
        index_type = vk::IndexType::eUint32;
    } else if (index_format == Format::R16_UINT) {
        index_type = vk::IndexType::eUint16;
    } else {
        auto error_message = std::format("Unsupported index buffer format {}", format_as(index_format));
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    command_buffer.bindIndexBuffer(**static_cast<const VulkanBuffer&>(buffer).buffer, offset, index_type);
}

void VulkanGraphicsCommandBuffer::SetVertexBuffers(std::uint8_t                                             start_binding,
                                                   std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
                                                   std::span<const std::size_t>                             offsets) {
    auto vk_buffers = buffers |
                      std::ranges::views::transform([](const auto& buffer) {
                          return **static_cast<const VulkanBuffer&>(buffer.get()).buffer;
                      }) |
                      std::ranges::to<std::pmr::vector<vk::Buffer>>();

    command_buffer.bindVertexBuffers(start_binding, vk_buffers, offsets);
}

void VulkanGraphicsCommandBuffer::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    command_buffer.pushDataEXT({.offset = 0, .data = {.address = &info, .size = sizeof(info)}});
}

void VulkanGraphicsCommandBuffer::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    command_buffer.draw(vertex_count, instance_count, first_vertex, first_instance);
}

void VulkanGraphicsCommandBuffer::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {
    command_buffer.drawIndexed(index_count, instance_count, first_index, base_vertex, first_instance);
}

void VulkanGraphicsCommandBuffer::CopyTextureRegion(const Texture&          src,
                                                    math::vec3i             src_offset,
                                                    Texture&                dst,
                                                    math::vec3i             dst_offset,
                                                    math::vec3u             extent,
                                                    TextureSubresourceLayer src_layer,
                                                    TextureSubresourceLayer dst_layer) {
    // TODO: implement copy_texture_region
    BlitTexture(src, src_offset, extent, dst, dst_offset, extent, src_layer, dst_layer);
    // copy_texture_region(command_buffer, src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

void VulkanGraphicsCommandBuffer::BlitTexture(const Texture&          src,
                                              math::vec3i             src_offset,
                                              math::vec3u             src_extent,
                                              Texture&                dst,
                                              math::vec3i             dst_offset,
                                              math::vec3u             dst_extent,
                                              TextureSubresourceLayer src_layer,
                                              TextureSubresourceLayer dst_layer) {
    auto& vk_src_image = static_cast<const VulkanImage&>(src);
    auto& vk_dst_image = static_cast<const VulkanImage&>(dst);

    const vk::ImageBlit2 region = {
        .srcSubresource = to_vk_image_subresource_layer(src_layer, src.GetDesc()),
        .srcOffsets     = {{
            to_vk_offset3D(src_offset),
            to_vk_offset3D({
                src_offset.x + static_cast<std::int32_t>(src_extent.x),
                src_offset.y + static_cast<std::int32_t>(src_extent.y),
                src_offset.z + static_cast<std::int32_t>(src_extent.z),
            }),
        }},
        .dstSubresource = to_vk_image_subresource_layer(dst_layer, dst.GetDesc()),
        .dstOffsets     = {{
            to_vk_offset3D(dst_offset),
            to_vk_offset3D({
                dst_offset.x + static_cast<std::int32_t>(dst_extent.x),
                dst_offset.y + static_cast<std::int32_t>(dst_extent.y),
                dst_offset.z + static_cast<std::int32_t>(dst_extent.z),
            }),
        }},
    };

    command_buffer.blitImage2({
        .srcImage       = vk_src_image.image_handle,
        .srcImageLayout = to_vk_image_layout(src.GetCurrentLayout()),
        .dstImage       = vk_dst_image.image_handle,
        .dstImageLayout = to_vk_image_layout(dst.GetCurrentLayout()),
        .regionCount    = 1,
        .pRegions       = &region,
    });
}

VulkanComputeCommandBuffer::VulkanComputeCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, const std::shared_ptr<spdlog::logger>& logger, VulkanBindlessUtils& bindings, TracyVkCtx tracy_context, std::string_view name)
    : ComputeCommandContext(name), m_Logger(logger), m_TracyCtx(tracy_context), m_BindlessUtils(bindings), command_buffer(create_command_buffer(device, pool, name)) {
}

void VulkanComputeCommandBuffer::Begin() {
    command_buffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    auto& vk_bindless_utils = m_BindlessUtils;

    vk_bindless_utils.Bind(command_buffer);

#ifdef TRACY_ENABLE
    m_TracyZone = std::make_unique<tracy::VkCtxScope>(
        m_TracyCtx,
        __LINE__,
        __FILE__,
        sizeof(__FILE__) - 1,
        __FUNCTION__,
        std::strlen(__FUNCTION__),
        m_Name.data(),
        m_Name.size(),
        *command_buffer,
        true);
#endif
}

void VulkanComputeCommandBuffer::End() {
    m_TracyZone.reset();
    TracyVkCollect(m_TracyCtx, *command_buffer);
    command_buffer.end();
}

void VulkanComputeCommandBuffer::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                                 std::span<const GPUBufferBarrier> buffer_barriers,
                                                 std::span<const TextureBarrier>   texture_barriers) {
    pipeline_barrier_fn(command_buffer, global_barriers, buffer_barriers, texture_barriers);
}

void VulkanComputeCommandBuffer::SetPipeline(const ComputePipeline& pipeline) {
    auto vk_pipeline = &static_cast<const VulkanComputePipeline&>(pipeline);
    if (m_Pipeline == vk_pipeline) return;
    m_Pipeline = vk_pipeline;
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, **m_Pipeline->pipeline);
}

void VulkanComputeCommandBuffer::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    command_buffer.pushDataEXT({.offset = 0, .data = {.address = &info, .size = sizeof(info)}});
}

VulkanTransferCommandBuffer::VulkanTransferCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& pool, const std::shared_ptr<spdlog::logger>& logger, TracyVkCtx tracy_context, std::string_view name)
    : CopyCommandContext(name), m_Logger(logger), m_TracyCtx(tracy_context), command_buffer(create_command_buffer(device, pool, name)) {
}

void VulkanTransferCommandBuffer::Begin() {
    command_buffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

#ifdef TRACY_ENABLE
    m_TracyZone = std::make_unique<tracy::VkCtxScope>(
        m_TracyCtx,
        __LINE__,
        __FILE__,
        sizeof(__FILE__) - 1,
        __FUNCTION__,
        std::strlen(__FUNCTION__),
        m_Name.data(),
        m_Name.size(),
        *command_buffer,
        true);
#endif
}

void VulkanTransferCommandBuffer::End() {
    m_TracyZone.reset();
    TracyVkCollect(m_TracyCtx, *command_buffer);
    command_buffer.end();
}

void VulkanTransferCommandBuffer::ResourceBarrier(std::span<const GlobalBarrier>    global_barriers,
                                                  std::span<const GPUBufferBarrier> buffer_barriers,
                                                  std::span<const TextureBarrier>   texture_barriers) {
    pipeline_barrier_fn(command_buffer, global_barriers, buffer_barriers, texture_barriers);

    // workaround for swapchain semaphore
    for (auto& texture_barrier : texture_barriers) {
        auto& vk_image = static_cast<VulkanImage&>(texture_barrier.texture);

        if (vk_image.swap_chain == nullptr) continue;

        if (texture_barrier.dst_layout == TextureLayout::Present) {
            swap_chain_presentable_semaphores.emplace_back(vk_image.swap_chain->GetSemaphores().presentable);
        } else if (texture_barrier.dst_layout == TextureLayout::CopyDst) {
            swap_chain_image_available_semaphore = vk_image.swap_chain->GetSemaphores().image_available;
        }
    }
}

void VulkanTransferCommandBuffer::CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dest_offset, std::size_t size) {
    command_buffer.copyBuffer(
        **static_cast<const VulkanBuffer&>(src).buffer,
        **static_cast<VulkanBuffer&>(dst).buffer,
        vk::BufferCopy{
            .srcOffset = src_offset,
            .dstOffset = dest_offset,
            .size      = size,
        });
}

void VulkanTransferCommandBuffer::CopyBufferToTexture(const GPUBuffer&        src,
                                                      std::size_t             src_offset,
                                                      Texture&                dst,
                                                      math::vec3i             dst_offset,
                                                      math::vec3u             extent,
                                                      TextureSubresourceLayer dst_layer) {
    auto& dst_texture = static_cast<const VulkanImage&>(dst);
    auto& src_buffer  = static_cast<const VulkanBuffer&>(src);

    const vk::BufferImageCopy buffer_image_copy{
        .bufferOffset      = src_offset,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = to_vk_image_subresource_layer(dst_layer, dst.GetDesc()),
        .imageOffset       = to_vk_offset3D(dst_offset),
        .imageExtent       = to_vk_extent3D(extent),
    };

    command_buffer.copyBufferToImage(
        **src_buffer.buffer,
        **dst_texture.image,
        vk::ImageLayout::eTransferDstOptimal,
        buffer_image_copy);
}

void VulkanTransferCommandBuffer::CopyTextureToBuffer(const Texture&          src,
                                                      math::vec3i             src_offset,
                                                      math::vec3u             extent,
                                                      GPUBuffer&              dst,
                                                      std::size_t             dst_offset,
                                                      TextureSubresourceLayer src_layer) {
    auto& src_image  = static_cast<const VulkanImage&>(src);
    auto& dst_buffer = static_cast<VulkanBuffer&>(dst);

    const vk::BufferImageCopy buffer_image_copy{
        .bufferOffset      = dst_offset,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = to_vk_image_subresource_layer(src_layer, src.GetDesc()),
        .imageOffset       = to_vk_offset3D(src_offset),
        .imageExtent       = to_vk_extent3D(extent),
    };

    command_buffer.copyImageToBuffer(
        src_image.image_handle,
        vk::ImageLayout::eTransferSrcOptimal,
        **dst_buffer.buffer,
        buffer_image_copy);
}

void VulkanTransferCommandBuffer::CopyTextureRegion(const Texture&          src,
                                                    math::vec3i             src_offset,
                                                    Texture&                dst,
                                                    math::vec3i             dst_offset,
                                                    math::vec3u             extent,
                                                    TextureSubresourceLayer src_layer,
                                                    TextureSubresourceLayer dst_layer) {
    copy_texture_region(command_buffer, src, src_offset, dst, dst_offset, extent, src_layer, dst_layer);
}

}  // namespace hitagi::gfx
