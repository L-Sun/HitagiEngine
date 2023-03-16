#pragma once
#include <hitagi/gfx/command_context.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

class VulkanTransferCommandBuffer final : public CopyCommandContext {
public:
    VulkanTransferCommandBuffer(VulkanDevice& device, std::string_view name);
    void SetName(std::string_view name) final;
    void ResetState(GPUBuffer& buffer) final;
    void ResetState(Texture& texture) final;

    void Begin() final;
    void End() final;

    void Reset() final;

    void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dest, std::size_t dest_offset, std::size_t size) final;
    void CopyTexture(const Texture& src, const Texture& dest) final;

    vk::raii::CommandBuffer command_buffer;
};

}  // namespace hitagi::gfx