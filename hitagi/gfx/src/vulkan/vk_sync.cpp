#include "vk_sync.hpp"
#include "vk_device.hpp"
#include "utils.hpp"

namespace hitagi::gfx {
const vk::StructureChain semaphore_create_info = {
    vk::SemaphoreCreateInfo{},
    vk::SemaphoreTypeCreateInfoKHR{
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue  = 0,
    },
};

VulkanSemaphore::VulkanSemaphore(VulkanDevice& device, std::string_view name)
    : Semaphore(device, name),
      semaphore(device.GetDevice(),
                semaphore_create_info.get(),
                device.GetCustomAllocator())

{
    device.GetDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
        .objectType   = vk::ObjectType::eSemaphore,
        .objectHandle = get_vk_handle(semaphore),
        .pObjectName  = m_Name.c_str(),
    });
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

}  // namespace hitagi::gfx