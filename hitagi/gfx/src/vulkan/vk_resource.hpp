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
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&)      = default;
    ~VulkanBuffer() final;

    auto GetMappedPtr() const noexcept -> std::byte* final;

    std::unique_ptr<vk::raii::Buffer> buffer;
    VmaAllocation                     allocation = nullptr;
    std::byte*                        mapped_ptr = nullptr;
};

struct VulkanImage final : public Texture {
    VulkanImage(VulkanDevice& device, TextureDesc desc, std::span<const std::byte> initial_data);
    VulkanImage(const VulkanSwapChain& swap_chian, std::uint32_t index);
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage(VulkanImage&&)      = default;
    ~VulkanImage() final;

    std::optional<vk::raii::Image>     image;
    vk::Image                          image_handle;
    std::optional<vk::raii::ImageView> image_view;

    VmaAllocation allocation = nullptr;
};

struct VulkanSampler final : public Sampler {
    VulkanSampler(VulkanDevice& device, SamplerDesc desc);

    std::unique_ptr<vk::raii::Sampler> sampler;
};

class VulkanSwapChain final : public SwapChain {
public:
    struct SemaphorePair {
        SemaphorePair(VulkanDevice& device, std::string_view name);
        std::shared_ptr<vk::raii::Semaphore> image_available;
        std::shared_ptr<vk::raii::Semaphore> presentable;
    };

    VulkanSwapChain(VulkanDevice& device, SwapChainDesc desc);

    inline auto GetWidth() const noexcept -> std::uint32_t final { return m_Size.x; };
    inline auto GetHeight() const noexcept -> std::uint32_t final { return m_Size.y; };
    inline auto GetFormat() const noexcept -> Format final { return m_Format; }

    void Present() final;
    void Resize() final;

    inline auto& GetVkSwapChain() const noexcept { return *m_SwapChain; }

    inline const auto& GetSemaphores() const noexcept { return m_SemaphorePair; }

    auto AcquireImageForRendering() -> VulkanImage&;

private:
    void CreateSwapChain();
    void CreateImageViews();

    std::unique_ptr<vk::raii::SurfaceKHR>   m_Surface;
    std::unique_ptr<vk::raii::SwapchainKHR> m_SwapChain;
    math::vec2u                             m_Size;
    Format                                  m_Format;
    std::uint32_t                           m_NumImages;
    std::pmr::vector<VulkanImage>           m_Images;

    int m_CurrentIndex = -1;

    SemaphorePair m_SemaphorePair;
};

struct VulkanShader final : public Shader {
    VulkanShader(VulkanDevice& device, ShaderDesc desc, std::span<const std::byte> binary_program);

    auto GetSPIRVData() const noexcept -> std::span<const std::byte> final;

    core::Buffer           binary_program;
    vk::raii::ShaderModule shader;

private:
    void Compile();
};

struct VulkanRenderPipeline final : public RenderPipeline {
    VulkanRenderPipeline(VulkanDevice& device, RenderPipelineDesc desc);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

struct VulkanComputePipeline final : public ComputePipeline {
    VulkanComputePipeline(VulkanDevice& device, ComputePipelineDesc desc);

    std::unique_ptr<vk::raii::Pipeline> pipeline;
};

}  // namespace hitagi::gfx