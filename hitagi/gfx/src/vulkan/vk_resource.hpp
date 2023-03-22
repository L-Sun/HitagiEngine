#pragma once
#include <hitagi/gfx/sync.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

#include <variant>

namespace hitagi::gfx {
class VulkanDevice;
class VulkanSwapChain;

struct VulkanBuffer final : public GPUBuffer {
    VulkanBuffer(VulkanDevice& device, GPUBufferDesc desc, std::span<const std::byte> initial_data);
    ~VulkanBuffer() final;

    auto GetMappedPtr() const noexcept -> std::byte* final;

    std::unique_ptr<vk::raii::Buffer> buffer;
    VmaAllocation                     allocation;
    std::byte*                        mapped_ptr = nullptr;
};

struct VulkanImage : public Texture {
    VulkanImage(VulkanDevice& device, TextureDesc desc, std::span<const std::byte> initial_data);
    VulkanImage(const VulkanSwapChain& swap_chian, std::uint32_t index);

    std::optional<vk::raii::Image>     image;
    vk::Image                          image_handle;
    std::optional<vk::raii::ImageView> image_view;
};

class VulkanSwapChain final : public SwapChain {
public:
    VulkanSwapChain(VulkanDevice& device, SwapChainDesc desc);
    ~VulkanSwapChain() final = default;

    auto AcquireNextTexture(
        utils::optional_ref<Semaphore> signal_semaphore = {},
        utils::optional_ref<Fence>     signal_fence     = {}) -> std::pair<std::reference_wrapper<Texture>, std::uint32_t> final;
    auto GetTexture(std::uint32_t index) -> Texture& final;
    auto GetTextures() -> std::pmr::vector<std::reference_wrapper<Texture>> final;

    inline auto GetWidth() const noexcept -> std::uint32_t final { return m_Size.x; };
    inline auto GetHeight() const noexcept -> std::uint32_t final { return m_Size.y; };
    inline auto GetFormat() const noexcept -> Format final { return m_Format; }

    void Present(std::uint32_t index, utils::optional_ref<Semaphore> wait_semaphore = {}) final;
    void Resize() final;

    inline auto& GetVkSwapChain() const noexcept { return *m_SwapChain; }

private:
    void CreateSwapChain();
    void CreateImageViews();

    std::unique_ptr<vk::raii::SurfaceKHR>   m_Surface;
    std::unique_ptr<vk::raii::SwapchainKHR> m_SwapChain;
    math::vec2u                             m_Size;
    Format                                  m_Format;
    std::uint32_t                           m_NumImages;
    std::pmr::vector<VulkanImage>           m_Images;
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

    std::pmr::vector<vk::PushConstantRange> push_constant_ranges;

    std::pmr::vector<vk::raii::DescriptorSetLayout> descriptor_set_layouts;
    std::unique_ptr<vk::raii::PipelineLayout>       pipeline_layout;
};

struct VulkanRenderPipeline : public RenderPipeline {
    VulkanRenderPipeline(VulkanDevice& device, RenderPipelineDesc desc);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

struct VulkanComputePipeline : public ComputePipeline {
    VulkanComputePipeline(VulkanDevice& device, ComputePipelineDesc desc);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

}  // namespace hitagi::gfx