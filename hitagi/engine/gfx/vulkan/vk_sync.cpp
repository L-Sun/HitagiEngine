module;
#include "interop/tracy_macros.hpp"

export module gfx.vulkan:sync;
import interop.tracy;
import interop.vulkan;
import interop.magic_enum;

import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :utils;
import :configs;

export namespace hitagi::gfx {

struct VulkanTimelineSemaphore final : public Fence {
public:
    VulkanTimelineSemaphore(const vk::raii::Device& device, const vk::AllocationCallbacks& allocator, std::uint64_t initial_value = 0, std::string_view name = "");
    ~VulkanTimelineSemaphore() final = default;

    void Signal(std::uint64_t value) final;
    bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)()) final;
    auto GetCurrentValue() -> std::uint64_t final;

    vk::raii::Semaphore timeline_semaphore;

private:
    const vk::raii::Device& m_Device;
};

}  // namespace hitagi::gfx

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

VulkanTimelineSemaphore::VulkanTimelineSemaphore(const vk::raii::Device& device, const vk::AllocationCallbacks& allocator, std::uint64_t initial_value, std::string_view name)
    : Fence(name), m_Device(device), timeline_semaphore(device, semaphore_create_info(initial_value).get(), allocator) {
    create_vk_debug_object_info(timeline_semaphore, name, device);
}

void VulkanTimelineSemaphore::Signal(std::uint64_t value) {
    m_Device.signalSemaphore(vk::SemaphoreSignalInfo{
        .semaphore = *timeline_semaphore,
        .value     = value,
    });
}

bool VulkanTimelineSemaphore::Wait(std::uint64_t value, std::chrono::milliseconds timeout) {
    ZoneScopedNS("VulkanTimelineSemaphore::Wait", 8);
    return m_Device.waitSemaphores(
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
