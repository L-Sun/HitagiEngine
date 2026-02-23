module;

#include <vulkan/vulkan_raii.hpp>
export module gfx.vulkan:command_queue;
import gfx.base;
import :resource;
import :sync;

export namespace hitagi::gfx {
class VulkanDevice;

class VulkanCommandQueue final : public CommandQueue {
public:
    VulkanCommandQueue(VulkanDevice& device, CommandType type, std::string_view name, std::uint32_t queue_family_index);
    ~VulkanCommandQueue() final = default;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> commands,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final;

    void WaitIdle() final;

    inline auto  GetFamilyIndex() const noexcept { return m_FamilyIndex; }
    inline auto& GetVkQueue() const noexcept { return m_Queue; }

private:
    std::uint32_t   m_FamilyIndex;
    vk::raii::Queue m_Queue;
};

}  // namespace hitagi::gfx
