#pragma once
#include <hitagi/gfx/gpu_resource.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanImage : public Texture {
    using Texture::Texture;
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