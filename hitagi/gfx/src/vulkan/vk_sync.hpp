#pragma once
#include <hitagi/gfx/sync.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanTimelineSemaphore final : public Fence {
public:
    VulkanTimelineSemaphore(VulkanDevice& device, std::uint64_t initial_value = 0, std::string_view name = "");
    ~VulkanTimelineSemaphore() final = default;

    void Signal(std::uint64_t value) final;
    bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) final;
    auto GetCurrentValue() -> std::uint64_t final;

    vk::raii::Semaphore timeline_semaphore;
};

}  // namespace hitagi::gfx