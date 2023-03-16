#include "vk_command_buffer.hpp"
#include "vk_device.hpp"
#include "vk_resource.hpp"
#include "utils.hpp"

namespace hitagi::gfx {
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
                        .front()))

{
    SetName(name);
}

void VulkanTransferCommandBuffer::SetName(std::string_view name) {
    m_Name = name;
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