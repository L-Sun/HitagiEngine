#pragma once
#include "vk_resource.hpp"

#include <hitagi/gfx/command_context.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

class VulkanGraphicsCommandBuffer final : public GraphicsCommandContext {
public:
    VulkanGraphicsCommandBuffer(VulkanDevice& device, std::string_view name);

    void Begin() final;
    void End() final;
    void Reset() final;

    void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) final;

    void BeginRendering(const RenderingInfo& info) final;
    void EndRendering() final;

    void SetPipeline(const RenderPipeline& pipeline) final;

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::vec4f& color) final;

    void SetIndexBuffer(GPUBuffer& buffer) final;
    void SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) final;

    void PushConstant(std::uint32_t slot, std::span<const std::byte> data) final;

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final;

    void CopyTexture(const Texture& src, Texture& dst) final;

    vk::raii::CommandBuffer command_buffer;

private:
    const VulkanRenderPipeline* m_Pipeline = nullptr;
};

class VulkanComputeCommandBuffer final : public ComputeCommandContext {
public:
    VulkanComputeCommandBuffer(VulkanDevice& device, std::string_view name);

    void Begin() final;
    void End() final;
    void Reset() final;

    void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) final;

    void SetPipeline(const ComputePipeline& pipeline) final;

    void PushConstant(std::uint32_t slot, std::span<const std::byte> data) final;

    vk::raii::CommandBuffer command_buffer;

private:
    const VulkanComputePipeline* m_Pipeline = nullptr;
};

class VulkanTransferCommandBuffer final : public CopyCommandContext {
public:
    VulkanTransferCommandBuffer(VulkanDevice& device, std::string_view name);

    void Begin() final;
    void End() final;
    void Reset() final;

    void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) final;

    void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) final;
    void CopyTexture(const Texture& src, Texture& dst) final;
    void CopyBufferToTexture(
        const GPUBuffer& src,
        std::size_t      src_offset,
        Texture&         dst,
        math::vec3i      dst_offset,
        math::vec3u      extent,
        std::uint32_t    mip_level        = 0,
        std::uint32_t    base_array_layer = 0,
        std::uint32_t    layer_count      = 1) final;

    vk::raii::CommandBuffer command_buffer;
};

}  // namespace hitagi::gfx