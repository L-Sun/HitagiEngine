#pragma once
#include <hitagi/gfx/sync.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

struct VulkanSemaphore final : public Semaphore {
public:
    VulkanSemaphore(VulkanDevice& device, std::uint64_t initial_value = 0, std::string_view name = "");
    ~VulkanSemaphore() final = default;

    // Host side
    void Signal(std::uint64_t value) final;
    void Wait(std::uint64_t value) final;
    bool WaitFor(std::uint64_t value, std::chrono::duration<double> timeout) final;
    auto GetCurrentValue() -> std::uint64_t final;

    vk::raii::Semaphore semaphore;
};

}  // namespace hitagi::gfx