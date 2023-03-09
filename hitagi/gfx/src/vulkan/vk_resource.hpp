#pragma once
#include <hitagi/gfx/gpu_resource.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanBuffer final : public GpuBuffer {
    VulkanBuffer(VulkanDevice& device, GpuBuffer::Desc desc, std::span<const std::byte> initial_data);
    ~VulkanBuffer() final;

    void UpdateRaw(std::size_t index, std::span<const std::byte> data) final;
    auto GetMappedPtr() const noexcept -> std::byte* final;

    std::unique_ptr<vk::raii::Buffer> buffer;
    VmaAllocation                     allocation;
    std::byte*                        mapped_ptr = nullptr;
};

struct VulkanImage : public Texture {
    VulkanImage(VulkanDevice& device, Texture::Desc desc);
    std::optional<vk::raii::Image>     image;
    std::optional<vk::raii::ImageView> image_view;
};

struct VulkanSwapChain final : public SwapChain {
    VulkanSwapChain(VulkanDevice& device, SwapChain::Desc desc);
    ~VulkanSwapChain() final = default;

    auto GetCurrentBackBuffer() -> Texture& final;
    auto GetBuffers() -> std::pmr::vector<std::reference_wrapper<Texture>> final;
    auto Width() -> std::uint32_t final;
    auto Height() -> std::uint32_t final;
    void Present() final;
    void Resize() final;

    void CreateSwapchain();
    void CreateImageViews();

    std::unique_ptr<vk::raii::SurfaceKHR>   surface;
    std::unique_ptr<vk::raii::SwapchainKHR> swapchain;
    math::vec2u                             size;

    std::pmr::vector<std::shared_ptr<VulkanImage>> images;
    std::pmr::vector<std::pmr::string>             image_names;
};
}  // namespace hitagi::gfx