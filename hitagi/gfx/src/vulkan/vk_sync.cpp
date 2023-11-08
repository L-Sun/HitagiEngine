#include "vk_sync.hpp"
#include "vk_device.hpp"
#include "vk_utils.hpp"

namespace hitagi::gfx {

auto semaphore_create_info(std::uint64_t initial_value) {
    return vk::StructureChain{
        vk::SemaphoreCreateInfo{},
        vk::SemaphoreTypeCreateInfoKHR{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue  = initial_value,
        },
    };
}

VulkanTimelineSemaphore::VulkanTimelineSemaphore(VulkanDevice& device, std::uint64_t initial_value, std::string_view name)
    : Fence(device, name),
      timeline_semaphore(
          device.GetDevice(),
          semaphore_create_info(initial_value).get(),
          device.GetCustomAllocator()) {
    create_vk_debug_object_info(timeline_semaphore, name, device.GetDevice());
}

void VulkanTimelineSemaphore::Signal(std::uint64_t value) {
    static_cast<VulkanDevice&>(m_Device).GetDevice().signalSemaphore(vk::SemaphoreSignalInfo{
        .semaphore = *timeline_semaphore,
        .value     = value,
    });
}

bool VulkanTimelineSemaphore::Wait(std::uint64_t value, std::chrono::milliseconds timeout) {
    return static_cast<VulkanDevice&>(m_Device).GetDevice().waitSemaphores(
               vk::SemaphoreWaitInfo{
                   .semaphoreCount = 1,
                   .pSemaphores    = &*timeline_semaphore,
                   .pValues        = &value,
               },
               std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count()) == vk::Result::eSuccess;
}

auto VulkanTimelineSemaphore::GetCurrentValue() -> std::uint64_t {
    return timeline_semaphore.getCounterValue();
}

}  // namespace hitagi::gfx