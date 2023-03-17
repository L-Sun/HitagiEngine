#include "vk_command_buffer.hpp"
#include "vk_device.hpp"
#include "vk_resource.hpp"
#include "utils.hpp"

#include <spdlog/logger.h>

namespace hitagi::gfx {
VulkanGraphicsCommandBuffer::VulkanGraphicsCommandBuffer(VulkanDevice& device, std::string_view name)
    : GraphicsCommandContext(device, CommandType::Graphics, name),
      command_buffer(std::move(
          vk::raii::CommandBuffers(
              device.GetDevice(),
              {
                  .commandPool        = *device.GetCommandPool(CommandType::Graphics),
                  .level              = vk::CommandBufferLevel::ePrimary,
                  .commandBufferCount = 1,
              })
              .front())) {
    create_vk_debug_object_info(command_buffer, m_Name, static_cast<VulkanDevice&>(m_Device).GetDevice());
}

void VulkanGraphicsCommandBuffer::Begin() {
    command_buffer.begin({
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
}

void VulkanGraphicsCommandBuffer::End() {
    command_buffer.end();
}

void VulkanGraphicsCommandBuffer::Reset() {
    command_buffer.reset();
}

void VulkanGraphicsCommandBuffer::SetViewPort(const ViewPort& view_port) {
    command_buffer.setViewport(0, to_vk_viewport(view_port));
}

void VulkanGraphicsCommandBuffer::SetScissorRect(const Rect& scissor_rect) {
    command_buffer.setScissor(
        0,
        vk::Rect2D{
            .offset = {scissor_rect.x, scissor_rect.y},
            .extent = {scissor_rect.width, scissor_rect.height},
        });
}

void VulkanGraphicsCommandBuffer::SetBlendColor(const math::vec4f& color) {
    command_buffer.setBlendConstants(color);
}

void VulkanGraphicsCommandBuffer::SetRenderTarget(Texture& target) {
    auto color_attachment = &static_cast<VulkanImage&>(target);
    command_buffer.s
        command_buffer.bindFramebuffer(vk::FramebufferBindPoint::eGraphics, **m_RenderTarget->framebuffer);
}

void VulkanGraphicsCommandBuffer::SetRenderTargetAndDepthStencil(Texture& target, Texture& depth_stencil) {}

void VulkanGraphicsCommandBuffer::ClearRenderTarget(Texture& target) {}

void VulkanGraphicsCommandBuffer::ClearDepthStencil(Texture& depth_stencil) {}

void VulkanGraphicsCommandBuffer::SetPipeline(const GraphicsPipeline& pipeline) {
    auto vk_pipeline = &static_cast<const VulkanGraphicsPipeline&>(pipeline);
    if (m_Pipeline == vk_pipeline) return;
    m_Pipeline = vk_pipeline;
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, **m_Pipeline->pipeline);
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

void VulkanGraphicsCommandBuffer::PushConstant(std::uint32_t slot, const std::span<const std::byte>& data) {
    vk::ArrayProxy<const std::byte> data_proxy(data.size(), data.data());
    // TODO get visibility from pipeline
    command_buffer.pushConstants(**m_Pipeline->pipeline_layout, vk::ShaderStageFlagBits::eAll, slot, data_proxy);
}

void VulkanGraphicsCommandBuffer::BindConstantBuffer(std::uint32_t slot, GPUBuffer& buffer, std::size_t index) {
}

void VulkanGraphicsCommandBuffer::BindTexture(std::uint32_t slot, Texture& texture) {}

void VulkanGraphicsCommandBuffer::Draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) {
    command_buffer.draw(vertex_count, instance_count, first_vertex, first_instance);
}

void VulkanGraphicsCommandBuffer::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::uint32_t base_vertex, std::uint32_t first_instance) {}

void VulkanGraphicsCommandBuffer::CopyTexture(const Texture& src, Texture& dest) {}

VulkanTransferCommandBuffer::VulkanTransferCommandBuffer(VulkanDevice& device, std::string_view name)
    : CopyCommandContext(device, CommandType::Copy, name),
      command_buffer(
          std::move(vk::raii::CommandBuffers(
                        device.GetDevice(),
                        {
                            .commandPool        = *device.GetCommandPool(CommandType::Copy),
                            .level              = vk::CommandBufferLevel::ePrimary,
                            .commandBufferCount = 1,
                        })
                        .front())) {
    create_vk_debug_object_info(command_buffer, m_Name, static_cast<VulkanDevice&>(m_Device).GetDevice());
}

void VulkanTransferCommandBuffer::ResetState(GPUBuffer& buffer) {}

void VulkanTransferCommandBuffer::ResetState(Texture& texture) {}

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

void VulkanTransferCommandBuffer::CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dest, std::size_t dest_offset, std::size_t size) {
    command_buffer.copyBuffer(
        **static_cast<const VulkanBuffer&>(src).buffer,
        **static_cast<VulkanBuffer&>(dest).buffer,
        vk::BufferCopy{
            .srcOffset = src_offset,
            .dstOffset = dest_offset,
            .size      = size,
        });
}

void VulkanTransferCommandBuffer::CopyTexture(const Texture& src, const Texture& dest) {}

}  // namespace hitagi::gfx