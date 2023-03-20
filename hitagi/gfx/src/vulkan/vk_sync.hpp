#pragma once
#include <hitagi/gfx/sync.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanFence final : public Fence {
public:
    VulkanFence(VulkanDevice& device, std::string_view name = "");
    ~VulkanFence() final = default;

    void Wait(std::chrono::duration<double> timeout = std::chrono::duration<double>::max()) final;

    vk::raii::Fence fence;
};

struct VulkanSemaphore final : public Semaphore {
    VulkanSemaphore(VulkanDevice& device, std::string_view name = "");
    ~VulkanSemaphore() final = default;

    vk::raii::Semaphore semaphore;
};

}  // namespace hitagi::gfx