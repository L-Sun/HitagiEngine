#include "vk_command_buffer.hpp"
#include "vk_device.hpp"
#include "vk_resource.hpp"
#include "vk_utils.hpp"
#include <hitagi/utils/exceptions.hpp>

#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <spdlog/logger.h>
#include <fmt/color.h>

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

inline auto create_command_buffer(const VulkanDevice& device, CommandType type, std::string_view name) -> vk::raii::CommandBuffer {
    vk::raii::CommandBuffer command_buffer = std::move(vk::raii::CommandBuffers(
                                                           device.GetDevice(),
                                                           {
                                                               .commandPool        = *device.GetCommandPool(type),
                                                               .level              = vk::CommandBufferLevel::ePrimary,
                                                               .commandBufferCount = 1,
                                                           })
                                                           .front());
    create_vk_debug_object_info(command_buffer, name, device.GetDevice());
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

VulkanGraphicsCommandBuffer::VulkanGraphicsCommandBuffer(VulkanDevice& device, std::string_view name)
    : GraphicsCommandContext(device, name),
      command_buffer(create_command_buffer(device, CommandType::Graphics, name)) {}

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
    const auto& vk_bindless_utils = static_cast<VulkanBindlessUtils&>(m_Device.GetBindlessUtils());

    std::pmr::vector<vk::DescriptorSet> descriptor_sets;
    std::transform(
        vk_bindless_utils.descriptor_sets.begin(),
        vk_bindless_utils.descriptor_sets.end(),
        std::back_inserter(descriptor_sets),
        [](const auto& descriptor_set) -> const vk::DescriptorSet { return *descriptor_set; });

    command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        **vk_bindless_utils.pipeline_layout,
        0,
        descriptor_sets,
        {});
}

void VulkanGraphicsCommandBuffer::End() {
    command_buffer.end();
}

void VulkanGraphicsCommandBuffer::BeginRendering(Texture& render_target, utils::optional_ref<Texture> depth_stencil, bool clear_render_target, bool clear_depth_stencil) {
    auto& vk_color_attachment_image = static_cast<VulkanImage&>(render_target);

    vk::RenderingAttachmentInfo color_attachment, depth_attachment, stencil_attachment;

    color_attachment = {
        .imageView   = *vk_color_attachment_image.image_view.value(),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = clear_render_target && vk_color_attachment_image.GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = clear_render_target && vk_color_attachment_image.GetDesc().clear_value
                           ? to_vk_clear_value(vk_color_attachment_image.GetDesc().clear_value.value())
                           : vk::ClearValue{},
    };
    if (depth_stencil.has_value()) {
        auto& vk_depth_stencil_image = static_cast<VulkanImage&>(depth_stencil->get());
        depth_attachment             = {
                        .imageView   = *vk_depth_stencil_image.image_view.value(),
                        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                        .loadOp      = clear_depth_stencil && depth_stencil->get().GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                        .storeOp     = vk::AttachmentStoreOp::eStore,
                        .clearValue  = clear_depth_stencil && depth_stencil->get().GetDesc().clear_value
                                           ? to_vk_clear_value(depth_stencil->get().GetDesc().clear_value.value())
                                           : vk::ClearValue{},
        };
        switch (depth_stencil->get().GetDesc().format) {
            case Format::D24_UNORM_S8_UINT:
            case Format::D32_FLOAT_S8X24_UINT: {
                stencil_attachment = {
                    .imageView   = *vk_depth_stencil_image.image_view.value(),
                    .imageLayout = vk::ImageLayout::eStencilAttachmentOptimal,
                    .loadOp      = clear_depth_stencil && depth_stencil->get().GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                    .storeOp     = vk::AttachmentStoreOp::eStore,
                    .clearValue  = clear_depth_stencil && depth_stencil->get().GetDesc().clear_value
                                       ? to_vk_clear_value(depth_stencil->get().GetDesc().clear_value.value())
                                       : vk::ClearValue{},
                };
            }
            default:
                break;
        }
    }

    command_buffer.beginRendering({
        .renderArea = {
            .offset = {0, 0},
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
            .offset = {static_cast<std::int32_t>(scissor_rect.x), static_cast<std::int32_t>(scissor_rect.y)},
            .extent = {scissor_rect.width, scissor_rect.height},
        });
}

void VulkanGraphicsCommandBuffer::SetBlendColor(const math::Color& color) {
    command_buffer.setBlendConstants(color);
}

void VulkanGraphicsCommandBuffer::SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset) {
    vk::IndexType index_type;
    if (buffer.GetDesc().element_size == sizeof(std::uint32_t)) {
        index_type = vk::IndexType::eUint32;
    } else if (buffer.GetDesc().element_size == sizeof(std::uint16_t)) {
        index_type = vk::IndexType::eUint16;
    } else {
        auto error_message = fmt::format("Unsupported index buffer which element size is {}", buffer.GetDesc().element_size);
        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }
    command_buffer.bindIndexBuffer(**static_cast<const VulkanBuffer&>(buffer).buffer, offset, index_type);
}

void VulkanGraphicsCommandBuffer::SetVertexBuffers(std::uint8_t                                             start_binding,
                                                   std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
                                                   std::span<const std::size_t>                             offsets) {
    auto vk_buffers = buffers |
                      ranges::views::transform([](const auto& buffer) {
                          return **static_cast<const VulkanBuffer&>(buffer.get()).buffer;
                      }) |
                      ranges::to<std::pmr::vector<vk::Buffer>>();

    command_buffer.bindVertexBuffers(start_binding, vk_buffers, offsets);
}

void VulkanGraphicsCommandBuffer::PushBindlessMetaInfo(const BindlessMetaInfo& info) {
    const auto& vk_bindless_utils = static_cast<VulkanBindlessUtils&>(m_Device.GetBindlessUtils());

    const vk::ArrayProxy<const BindlessMetaInfo> data_proxy(info);

    command_buffer.pushConstants(
        **vk_bindless_utils.pipeline_layout,
        vk_bindless_utils.bindless_info_constant_range.stageFlags,
        vk_bindless_utils.bindless_info_constant_range.offset,
        data_proxy);
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

VulkanComputeCommandBuffer::VulkanComputeCommandBuffer(VulkanDevice& device, std::string_view name)
    : ComputeCommandContext(device, name),
      command_buffer(create_command_buffer(device, CommandType::Compute, name)) {
}

void VulkanComputeCommandBuffer::Begin() {
    command_buffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    const auto& vk_bindless_utils = static_cast<VulkanBindlessUtils&>(m_Device.GetBindlessUtils());

    std::pmr::vector<vk::DescriptorSet> descriptor_sets;
    std::transform(
        vk_bindless_utils.descriptor_sets.begin(),
        vk_bindless_utils.descriptor_sets.end(),
        std::back_inserter(descriptor_sets),
        [](const auto& descriptor_set) -> const vk::DescriptorSet { return *descriptor_set; });

    command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        **vk_bindless_utils.pipeline_layout,
        0,
        descriptor_sets,
        {});
}

void VulkanComputeCommandBuffer::End() {
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
    const auto& vk_bindless_utils = static_cast<VulkanBindlessUtils&>(m_Device.GetBindlessUtils());

    const vk::ArrayProxy<const BindlessMetaInfo> data_proxy(info);

    command_buffer.pushConstants(
        **vk_bindless_utils.pipeline_layout,
        vk_bindless_utils.bindless_info_constant_range.stageFlags,
        vk_bindless_utils.bindless_info_constant_range.offset,
        data_proxy);
}

VulkanTransferCommandBuffer::VulkanTransferCommandBuffer(VulkanDevice& device, std::string_view name)
    : CopyCommandContext(device, name),
      command_buffer(create_command_buffer(device, CommandType::Copy, name)) {
}

void VulkanTransferCommandBuffer::Begin() {
    command_buffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
}

void VulkanTransferCommandBuffer::End() {
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