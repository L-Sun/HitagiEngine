#include "vk_sync.hpp"
#include "vk_device.hpp"
#include "utils.hpp"

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

VulkanSemaphore::VulkanSemaphore(VulkanDevice& device, std::uint64_t initial_value, std::string_view name)
    : Semaphore(device, name),
      semaphore(device.GetDevice(),
                semaphore_create_info(initial_value).get(),
                device.GetCustomAllocator())

{
    create_vk_debug_object_info(semaphore, name, device.GetDevice());
}

void VulkanSemaphore::Signal(std::uint64_t value) {
    auto& vk_device = static_cast<VulkanDevice&>(m_Device).GetDevice();
    vk_device.signalSemaphore(vk::SemaphoreSignalInfo{
        .semaphore = *semaphore,
        .value     = value,
    });
}

void VulkanSemaphore::Wait(std::uint64_t value) {
    auto&                 vk_device = static_cast<VulkanDevice&>(m_Device).GetDevice();
    vk::SemaphoreWaitInfo wait_info{
        .semaphoreCount = 1,
        .pSemaphores    = &(*semaphore),
        .pValues        = &value,
    };
    while (vk::Result::eTimeout == vk_device.waitSemaphores(wait_info, std::numeric_limits<std::uint64_t>::max()))
        ;
}

bool VulkanSemaphore::WaitFor(std::uint64_t value, std::chrono::duration<double> timeout) {
    auto&                 vk_device = static_cast<VulkanDevice&>(m_Device).GetDevice();
    vk::SemaphoreWaitInfo wait_info{
        .semaphoreCount = 1,
        .pSemaphores    = &(*semaphore),
        .pValues        = &value,
    };
    return vk::Result::eTimeout != vk_device.waitSemaphores(wait_info, std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count());
}

auto VulkanSemaphore::GetCurrentValue() -> std::uint64_t {
    return semaphore.getCounterValue();
}

}  // namespace hitagi::gfx