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

    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final;

    void BeginRendering(Texture&                     render_target,
                        utils::optional_ref<Texture> depth_stencil       = {},
                        bool                         clear_render_target = false,
                        bool                         clear_depth_stencil = false) final;
    void EndRendering() final;

    void SetPipeline(const RenderPipeline& pipeline) final;

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::Color& color) final;

    void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset = 0) final;
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

    vk::raii::CommandBuffer              command_buffer;
    std::shared_ptr<vk::raii::Semaphore> swap_chain_image_available_semaphore;
    // get swapchain semaphore whose image whose layout transition to present
    std::pmr::vector<std::shared_ptr<vk::raii::Semaphore>> swap_chain_presentable_semaphores;

private:
    const VulkanRenderPipeline* m_Pipeline = nullptr;
};

class VulkanComputeCommandBuffer final : public ComputeCommandContext {
public:
    VulkanComputeCommandBuffer(VulkanDevice& device, std::string_view name);

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
    const VulkanComputePipeline* m_Pipeline = nullptr;
};

class VulkanTransferCommandBuffer final : public CopyCommandContext {
public:
    VulkanTransferCommandBuffer(VulkanDevice& device, std::string_view name);

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

    void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) final;

    vk::raii::CommandBuffer command_buffer;

    std::shared_ptr<vk::raii::Semaphore> swap_chain_image_available_semaphore;
    // get swapchain semaphore whose image whose layout transition to present
    std::pmr::vector<std::shared_ptr<vk::raii::Semaphore>> swap_chain_presentable_semaphores;
};

}  // namespace hitagi::gfx