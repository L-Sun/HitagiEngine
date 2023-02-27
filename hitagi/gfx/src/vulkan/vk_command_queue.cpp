#include "vk_command_queue.hpp"
#include "vk_device.hpp"

namespace hitagi::gfx {
constexpr std::array graphics_color = {0.48f, 0.76f, 0.47f, 1.0f};
constexpr std::array compute_color  = {0.38f, 0.69f, 0.94f, 1.0f};
constexpr std::array copy_color     = {0.88f, 0.39f, 0.35f, 1.0f};

VulkanCommandQueue::VulkanCommandQueue(VulkanDevice& device, CommandType type, std::string_view name, std::uint32_t queue_family_index)
    : CommandQueue(device, type, name),
      m_FamilyIndex(queue_family_index),
      m_Queue(device.GetDevice().getQueue(queue_family_index, magic_enum::enum_integer(type)))

{
    m_Queue.insertDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT{
        .pLabelName = name.data(),
        .color      = type == CommandType::Graphics  ? graphics_color
                      : type == CommandType::Compute ? compute_color
                                                     : copy_color,
    });
}

auto VulkanCommandQueue::Submit(std::pmr::vector<CommandContext*> context) -> std::uint64_t {
    return 0;
}

bool VulkanCommandQueue::IsFenceComplete(std::uint64_t fence_value) {
    return false;
}

void VulkanCommandQueue::WaitForFence(std::uint64_t fence_value) {
}

void VulkanCommandQueue::WaitForQueue(const CommandQueue& other) {
}

void VulkanCommandQueue::WaitIdle() {
}

}  // namespace hitagi::gfx