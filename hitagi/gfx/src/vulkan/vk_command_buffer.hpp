#pragma once
#include "vk_resource.hpp"

#include <hitagi/gfx/command_context.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

class VulkanGraphicsCommandBuffer final : public GraphicsCommandContext {
public:
    VulkanGraphicsCommandBuffer(VulkanDevice& device, std::string_view name);

    void ResetState(GPUBuffer& buffer) final;
    void ResetState(Texture& texture) final;

    void Begin() final;
    void End() final;
    void Reset() final;

    void SetPipeline(const GraphicsPipeline& pipeline) final;

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::vec4f& color) final;
    void SetRenderTarget(Texture& target) final;
    void SetRenderTargetAndDepthStencil(Texture& target, Texture& depth_stencil) final;

    void ClearRenderTarget(Texture& target) final;
    void ClearDepthStencil(Texture& depth_stencil) final;

    void SetIndexBuffer(GPUBuffer& buffer) final;
    void SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) final;

    void BindConstantBuffer(std::uint32_t slot, GPUBuffer& buffer, std::size_t index = 0) final;
    void BindTexture(std::uint32_t slot, Texture& texture) final;

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final;

    void Present(Texture& texture) final;

    void CopyTexture(const Texture& src, Texture& dest) final;

    vk::raii::CommandBuffer command_buffer;

private:
    const VulkanPipelineLayout*   m_PipelineLayout = nullptr;
    const VulkanGraphicsPipeline* m_Pipeline       = nullptr;
};

class VulkanTransferCommandBuffer final : public CopyCommandContext {
public:
    VulkanTransferCommandBuffer(VulkanDevice& device, std::string_view name);
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