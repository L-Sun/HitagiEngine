#include "vk_command_buffer.hpp"
#include "vk_device.hpp"
#include "vk_resource.hpp"
#include "vk_utils.hpp"
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::gfx {

inline void pipeline_barrier_fn(vk::raii::CommandBuffer&                  command_buffer,
                                const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                const std::pmr::vector<TextureBarrier>&   texture_barriers) {
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

VulkanGraphicsCommandBuffer::VulkanGraphicsCommandBuffer(VulkanDevice& device, std::string_view name)
    : GraphicsCommandContext(device, name),
      command_buffer(create_command_buffer(device, CommandType::Graphics, name)) {}

void VulkanGraphicsCommandBuffer::ResourceBarrier(const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                                  const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                                  const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    pipeline_barrier_fn(command_buffer, global_barriers, buffer_barriers, texture_barriers);
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

void VulkanGraphicsCommandBuffer::Reset() {
    command_buffer.reset();
}

void VulkanGraphicsCommandBuffer::BeginRendering(const RenderingInfo& render_info) {
    const auto& render_target = render_info.render_target;

    VulkanImage* vk_color_attachment_image;
    if (std::holds_alternative<std::reference_wrapper<Texture>>(render_target)) {
        vk_color_attachment_image = &static_cast<VulkanImage&>(std::get<std::reference_wrapper<Texture>>((render_target)).get());
    } else {
        auto& vk_swap_chain = static_cast<VulkanSwapChain&>(std::get<std::reference_wrapper<SwapChain>>(render_target).get());

        vk_color_attachment_image            = &vk_swap_chain.AcquireImageForRendering();
        swap_chain_image_available_semaphore = vk_swap_chain.GetSemaphores().image_available;

        ResourceBarrier(
            {}, {},
            {
                TextureBarrier{
                    .src_access = BarrierAccess::Unkown,
                    .dst_access = BarrierAccess::RenderTarget,
                    .src_stage  = PipelineStage::Render,
                    .dst_stage  = PipelineStage::Render,
                    .src_layout = TextureLayout::Unkown,
                    .dst_layout = TextureLayout::RenderTarget,
                    .texture    = *vk_color_attachment_image,
                },
            });
    }

    vk::RenderingAttachmentInfo color_attachment, depth_stencil_attachment;

    color_attachment = {
        .imageView   = *vk_color_attachment_image->image_view.value(),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk_color_attachment_image->GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = vk_color_attachment_image->GetDesc().clear_value
                           ? to_vk_clear_value(vk_color_attachment_image->GetDesc().clear_value.value())
                           : vk::ClearValue{},
    };
    if (render_info.depth_stencil.has_value()) {
        auto& vk_depth_stencil_image = static_cast<VulkanImage&>(render_info.depth_stencil->get());
        depth_stencil_attachment     = {
                .imageView   = *vk_depth_stencil_image.image_view.value(),
                .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                .loadOp      = render_info.depth_stencil->get().GetDesc().clear_value ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
                .storeOp     = vk::AttachmentStoreOp::eStore,
                .clearValue  = render_info.depth_stencil->get().GetDesc().clear_value
                                   ? to_vk_clear_value(render_info.depth_stencil->get().GetDesc().clear_value.value())
                                   : vk::ClearValue{},
        };
    }

    command_buffer.beginRendering({
        .renderArea = {
            .offset = {0, 0},
            .extent = {
                .width  = vk_color_attachment_image->GetDesc().width,
                .height = vk_color_attachment_image->GetDesc().height,
            },
        },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
        .pDepthAttachment     = &depth_stencil_attachment,
        .pStencilAttachment   = &depth_stencil_attachment,
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

void VulkanGraphicsCommandBuffer::SetBlendColor(const math::vec4f& color) {
    command_buffer.setBlendConstants(color);
}

void VulkanGraphicsCommandBuffer::SetIndexBuffer(GPUBuffer& buffer) {
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
    command_buffer.bindIndexBuffer(**static_cast<VulkanBuffer&>(buffer).buffer, 0, index_type);
}

void VulkanGraphicsCommandBuffer::SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) {
    command_buffer.bindVertexBuffers(slot, **static_cast<VulkanBuffer&>(buffer).buffer, {0});
}

void VulkanGraphicsCommandBuffer::PushBindlessInfo(const BindlessInfoOffset& bindless_info) {
    const auto& vk_bindless_utils = static_cast<VulkanBindlessUtils&>(m_Device.GetBindlessUtils());

    const vk::ArrayProxy<const BindlessInfoOffset> data_proxy(bindless_info);

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

void VulkanGraphicsCommandBuffer::Present(SwapChain& swap_chain) {
    auto& vk_swap_chain = static_cast<VulkanSwapChain&>(swap_chain);

    swap_chain_presentable_semaphore = vk_swap_chain.GetSemaphores().presentable;

    ResourceBarrier(
        {}, {},
        {
            TextureBarrier{
                .src_access = BarrierAccess::RenderTarget,
                .dst_access = BarrierAccess::Present,
                .src_stage  = PipelineStage::Render,
                .dst_stage  = PipelineStage::All,
                .src_layout = TextureLayout::RenderTarget,
                .dst_layout = TextureLayout::Present,
                .texture    = vk_swap_chain.AcquireImageForRendering(),
            },
        });
}

void VulkanGraphicsCommandBuffer::CopyTexture(const Texture& src, Texture& dst) {
    utils::NoImplemented();
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

void VulkanComputeCommandBuffer::Reset() {
    command_buffer.reset({});
}

void VulkanComputeCommandBuffer::ResourceBarrier(const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                                 const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                                 const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    pipeline_barrier_fn(command_buffer, global_barriers, buffer_barriers, texture_barriers);
}

void VulkanComputeCommandBuffer::SetPipeline(const ComputePipeline& pipeline) {
    auto vk_pipeline = &static_cast<const VulkanComputePipeline&>(pipeline);
    if (m_Pipeline == vk_pipeline) return;
    m_Pipeline = vk_pipeline;
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, **m_Pipeline->pipeline);
}

void VulkanComputeCommandBuffer::PushBindlessInfo(const BindlessInfoOffset& bindless_info) {
    const auto& vk_bindless_utils = static_cast<VulkanBindlessUtils&>(m_Device.GetBindlessUtils());

    const vk::ArrayProxy<const BindlessInfoOffset> data_proxy(bindless_info);

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

void VulkanTransferCommandBuffer::Reset() {
    command_buffer.reset({});
}

void VulkanTransferCommandBuffer::ResourceBarrier(const std::pmr::vector<GlobalBarrier>&    global_barriers,
                                                  const std::pmr::vector<GPUBufferBarrier>& buffer_barriers,
                                                  const std::pmr::vector<TextureBarrier>&   texture_barriers) {
    pipeline_barrier_fn(command_buffer, global_barriers, buffer_barriers, texture_barriers);
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

void VulkanTransferCommandBuffer::CopyTexture(const Texture& src, Texture& dst) {
    throw utils::NoImplemented();
}

void VulkanTransferCommandBuffer::CopyBufferToTexture(const GPUBuffer& src,
                                                      std::size_t      src_offset,
                                                      Texture&         dst,
                                                      math::vec3i      dst_offset,
                                                      math::vec3u      extent,
                                                      std::uint32_t    mip_level,
                                                      std::uint32_t    base_array_layer,
                                                      std::uint32_t    layer_count) {
    auto& dst_texture = static_cast<const VulkanImage&>(dst);
    auto& src_buffer  = static_cast<const VulkanBuffer&>(src);

    auto buffer_size    = src_buffer.GetDesc().element_size * src_buffer.GetDesc().element_count;
    auto copy_data_size = extent.x * extent.y * extent.z * get_format_byte_size(dst_texture.GetDesc().format);

    if (src_offset + copy_data_size > buffer_size) {
        auto error_message = fmt::format(
            "Buffer size is too small to copy to texture. Buffer size: {}, Buffer Offset: {}, Copy size: {}",
            fmt::styled(buffer_size, fmt::fg(fmt::color::red)),
            fmt::styled(src_offset, fmt::fg(fmt::color::red)),
            fmt::styled(copy_data_size, fmt::fg(fmt::color::red)));

        m_Device.GetLogger()->error(error_message);
        throw std::runtime_error(error_message);
    }

    const vk::BufferImageCopy buffer_image_copy{
        .bufferOffset      = src_offset,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = vk::ImageSubresourceLayers{
             .aspectMask     = to_vk_image_aspect(dst.GetDesc().usages),
             .mipLevel       = mip_level,
             .baseArrayLayer = base_array_layer,
             .layerCount     = layer_count,
        },
        .imageOffset = to_vk_offset3D(dst_offset),
        .imageExtent = to_vk_extent3D(extent),
    };

    command_buffer.copyBufferToImage(
        **src_buffer.buffer,
        **dst_texture.image,
        vk::ImageLayout::eTransferDstOptimal,
        buffer_image_copy);
}

}  // namespace hitagi::gfx