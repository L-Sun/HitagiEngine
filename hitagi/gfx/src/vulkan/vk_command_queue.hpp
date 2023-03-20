#pragma once
#include <hitagi/gfx/command_queue.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace hitagi::gfx {
class VulkanDevice;

class VulkanCommandQueue final : public CommandQueue {
public:
    VulkanCommandQueue(VulkanDevice& device, CommandType type, std::string_view name, std::uint32_t queue_family_index);
    ~VulkanCommandQueue() final = default;

    void Submit(
        std::pmr::vector<CommandContext*>                         contexts,
        std::pmr::vector<std::reference_wrapper<const Semaphore>> wait_semaphore   = {},
        std::pmr::vector<std::reference_wrapper<const Semaphore>> signal_semaphore = {},
        utils::optional_ref<const Fence>                          signal_fence     = {}) final;

    void WaitIdle() final;

    inline auto  GetFamilyIndex() const noexcept { return m_FamilyIndex; }
    inline auto& GetVkQueue() const noexcept { return m_Queue; }

private:
    std::uint32_t   m_FamilyIndex;
    vk::raii::Queue m_Queue;

    std::uint64_t m_SubmitCount = 0;
};

}  // namespace hitagi::gfx
