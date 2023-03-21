#pragma once
#include <hitagi/gfx/sync.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

#include <variant>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanBuffer final : public GPUBuffer {
    VulkanBuffer(VulkanDevice& device, GPUBufferDesc desc, std::span<const std::byte> initial_data);
    ~VulkanBuffer() final;

    auto GetMappedPtr() const noexcept -> std::byte* final;

    std::unique_ptr<vk::raii::Buffer> buffer;
    VmaAllocation                     allocation;
    std::byte*                        mapped_ptr = nullptr;
};

struct VulkanImage : public Texture {
    VulkanImage(VulkanDevice& device, TextureDesc desc);
    std::optional<vk::raii::Image>     image;
    vk::Image                          image_handle;
    std::optional<vk::raii::ImageView> image_view;
};

struct VulkanSwapChain final : public SwapChain {
    VulkanSwapChain(VulkanDevice& device, SwapChainDesc desc);
    ~VulkanSwapChain() final = default;

    auto AcquireNextTexture(
        utils::optional_ref<Semaphore> signal_semaphore = {},
        utils::optional_ref<Fence>     signal_fence     = {}) -> std::pair<std::reference_wrapper<Texture>, std::uint32_t> final;

    auto GetBuffer(std::uint32_t index) const -> Texture& final;
    auto GetTextures() const -> std::pmr::vector<std::reference_wrapper<Texture>> final;

    inline auto GetWidth() const noexcept -> std::uint32_t final { return size.x; };
    inline auto GetHeight() const noexcept -> std::uint32_t final { return size.y; };
    inline auto GetFormat() const noexcept -> Format final { return format; }

    void Present(std::uint32_t index, utils::optional_ref<Semaphore> wait_semaphore = {}) final;
    void Resize() final;

    std::unique_ptr<vk::raii::SurfaceKHR>          surface;
    std::unique_ptr<vk::raii::SwapchainKHR>        swap_chain;
    math::vec2u                                    size;
    Format                                         format;
    std::pmr::vector<std::shared_ptr<VulkanImage>> images;
    // Binary semaphore
    vk::raii::Semaphore semaphore;
    std::uint32_t       crrent_index;

private:
    void CreateSwapChain();
    void CreateImageViews();
};

struct VulkanShader : public Shader {
    VulkanShader(VulkanDevice& device, ShaderDesc desc, std::span<const std::byte> binary_program);

    auto GetSPIRVData() const noexcept -> std::span<const std::byte> final;

    core::Buffer           binary_program;
    vk::raii::ShaderModule shader;

private:
    void Compile();
};

struct VulkanPipelineLayout : public RootSignature {
    VulkanPipelineLayout(VulkanDevice& device, RootSignatureDesc desc);

    // For bindless
    VulkanPipelineLayout(VulkanDevice& device, std::span<vk::DescriptorPoolSize> pool_sizes);

    std::pmr::vector<vk::raii::DescriptorSetLayout> descriptor_set_layouts;
    std::unique_ptr<vk::raii::PipelineLayout>       pipeline_layout;
};

struct VulkanGraphicsPipeline : public GraphicsPipeline {
    VulkanGraphicsPipeline(VulkanDevice& device, GraphicsPipelineDesc desc);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

}  // namespace hitagi::gfx