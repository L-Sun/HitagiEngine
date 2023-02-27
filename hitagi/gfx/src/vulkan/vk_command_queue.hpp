#pragma once
#include <hitagi/gfx/command_queue.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

class VulkanCommandQueue final : public CommandQueue {
public:
    VulkanCommandQueue(VulkanDevice& device, CommandType type, std::string_view name, std::uint32_t queue_family_index);
    ~VulkanCommandQueue() final = default;

    auto Submit(std::pmr::vector<CommandContext*> context) -> std::uint64_t final;
    bool IsFenceComplete(std::uint64_t fence_value) final;
    void WaitForFence(std::uint64_t fence_value) final;
    void WaitForQueue(const CommandQueue& other) final;
    void WaitIdle() final;

    inline auto& GetVkQueue() const noexcept { return m_Queue; }

private:
    vk::raii::Queue m_Queue;
};

}  // namespace hitagi::gfx