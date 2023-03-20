#include "vk_sync.hpp"
#include "vk_device.hpp"
#include "utils.hpp"

namespace hitagi::gfx {

VulkanFence::VulkanFence(VulkanDevice& device, std::string_view name)
    : Fence(device, name),
      fence(device.GetDevice(), vk::FenceCreateInfo{}, device.GetCustomAllocator())

{
    create_vk_debug_object_info(fence, name, device.GetDevice());
}

void VulkanFence::Wait(std::chrono::duration<double> timeout) {
    auto& vk_device = static_cast<VulkanDevice&>(m_Device).GetDevice();
    // nano seconds
    while (vk_device.waitForFences(
               *fence, true,
               std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count()) ==
           vk::Result::eTimeout)
        ;
}

VulkanSemaphore::VulkanSemaphore(VulkanDevice& device, std::string_view name)
    : Semaphore(device, name),
      semaphore(device.GetDevice(),
                vk::SemaphoreCreateInfo{},
                device.GetCustomAllocator()) {
    create_vk_debug_object_info(semaphore, name, device.GetDevice());
}

}  // namespace hitagi::gfx