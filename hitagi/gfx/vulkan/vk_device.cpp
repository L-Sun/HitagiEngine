module;
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#include <SDL3/SDL_vulkan.h>
#include <spirv_reflect.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_wayland.h>
#endif
#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vulkan/vulkan.hpp>

export module gfx.vulkan:device;
import std;
import core;
import utils;
import math;
import gfx.base;
import magic_enum;
import :types;
import :utils;
import :configs;

export namespace hitagi::gfx {

class VulkanDevice final : public Device {
public:
    VulkanDevice(std::string_view name);
    ~VulkanDevice() final;

    void Tick() final;

    inline auto& GetInstance() const noexcept { return *m_Instance; }
    inline auto& GetCustomAllocator() const noexcept { return m_CustomAllocator; }
    inline auto& GetPhysicalDevice() const noexcept { return *m_PhysicalDevice; }
    inline auto& GetDevice() const noexcept { return *m_Device; }
    inline auto  GetQueueFamilyIndex() const noexcept { return m_QueueFamilyIndex; }
    inline auto& GetVmaAllocator() const noexcept { return m_VmaAllocator; }

private:
    auto GetStorageBufferViewRequirements() const noexcept -> StorageViewRequirements final {
        return {.offset_alignment = std::max(std::uint64_t{4}, m_PhysicalDevice->getProperties().limits.minStorageBufferOffsetAlignment), .size_alignment = 4};
    }

    void Profile() const;

    vk::AllocationCallbacks m_CustomAllocator;
    using AllocationRecord = std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>>;
    AllocationRecord m_CustomAllocationRecord;

    vk::raii::Context                   m_Context;
    std::unique_ptr<vk::raii::Instance> m_Instance;

#ifdef HITAGI_DEBUG
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugUtilsMessenger;
#endif

    std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice;
    std::unique_ptr<vk::raii::Device>         m_Device;

    VmaAllocator  m_VmaAllocator;
    std::uint32_t m_QueueFamilyIndex;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanDevice::VulkanDevice(std::string_view name)
    : Device(Device::Type::Vulkan, name),
      m_CustomAllocator{
          .pUserData       = &m_CustomAllocationRecord,
          .pfnAllocation   = custom_vk_allocation_fn,
          .pfnReallocation = custom_vk_reallocation_fn,
          .pfnFree         = custom_vk_free_fn,
      }

{
    m_Logger->trace("Create Vulkan Instance...");
    {
        const vk::ApplicationInfo app_info{
            .pApplicationName   = "Hitagi",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName        = "Hitagi",
            .engineVersion      = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion         = m_Context.enumerateInstanceVersion(),
        };
        m_Logger->trace("Vulkan API Version: {}.{}.{}",
                        VK_VERSION_MAJOR(app_info.apiVersion),
                        VK_VERSION_MINOR(app_info.apiVersion),
                        VK_VERSION_PATCH(app_info.apiVersion));

        m_Instance = std::make_unique<vk::raii::Instance>(
            m_Context,
            vk::InstanceCreateInfo{
                .pApplicationInfo        = &app_info,
                .enabledLayerCount       = static_cast<std::uint32_t>(required_instance_layers.size()),
                .ppEnabledLayerNames     = required_instance_layers.data(),
                .enabledExtensionCount   = static_cast<std::uint32_t>(required_instance_extensions.size()),
                .ppEnabledExtensionNames = required_instance_extensions.data(),
            },
            GetCustomAllocator());
    }

#ifdef HITAGI_DEBUG
    m_Logger->trace("Enable validation message logger...");
    {
        m_DebugUtilsMessenger = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(
            *m_Instance,
            vk::DebugUtilsMessengerCreateInfoEXT{
                .messageSeverity =
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,

                .messageType =
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                .pfnUserCallback = custom_debug_message_fn,
                .pUserData       = m_Logger.get(),
            },
            GetCustomAllocator());
    }
#endif

    m_Logger->trace("Pick GPU...");
    {
        auto physical_devices = m_Instance->enumeratePhysicalDevices();
        m_Logger->trace("Found {} Vulkan physical devices", physical_devices.size());
        for (const auto& device : physical_devices) {
            m_Logger->trace("\t - {}", device.getProperties().deviceName.data());
        }

        std::erase_if(physical_devices, [](const auto& device) { return !is_physical_suitable(device); });
        std::ranges::sort(physical_devices, std::ranges::greater(), compute_physical_device_score);
        if (physical_devices.empty()) {
            throw std::runtime_error("No Vulkan device supports the required Descriptor Heap features and queues");
        }
        m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(std::move(physical_devices.front()));
        m_Logger->trace(
            "Pick physical device: {}",
            fmt::styled(m_PhysicalDevice->getProperties().deviceName.data(), fmt::fg(fmt::color::green)));
    }

    m_Logger->trace("Create logical device...");
    {
        const auto queue_create_info = get_queue_create_info(*m_PhysicalDevice).value();

        const vk::StructureChain device_create_info = {
            vk::DeviceCreateInfo{
                .queueCreateInfoCount    = 1,
                .pQueueCreateInfos       = &queue_create_info,
                .enabledExtensionCount   = static_cast<std::uint32_t>(required_device_extensions.size()),
                .ppEnabledExtensionNames = required_device_extensions.data(),
            },
            vk::PhysicalDeviceVulkan12Features{
                .shaderSampledImageArrayNonUniformIndexing  = true,
                .shaderStorageBufferArrayNonUniformIndexing = true,
                .shaderStorageImageArrayNonUniformIndexing  = true,
                .runtimeDescriptorArray                     = true,
                .timelineSemaphore                          = true,
                .bufferDeviceAddress                        = true,
            },
            vk::PhysicalDeviceDescriptorHeapFeaturesEXT{
                .descriptorHeap = true,
            },
            vk::PhysicalDeviceVulkan13Features{
                .synchronization2 = true,
                .dynamicRendering = true,
            },
            vk::PhysicalDeviceHostImageCopyFeaturesEXT{
                .hostImageCopy = true,
            },
        };

        m_Device = std::make_unique<vk::raii::Device>(m_PhysicalDevice->createDevice(device_create_info.get(), GetCustomAllocator()));

        m_QueueFamilyIndex = queue_create_info.queueFamilyIndex;
    }

    m_Logger->trace("Create VMA Allocator...");
    {
        const VmaAllocatorCreateInfo allocator_info = {
            .flags                = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice       = **m_PhysicalDevice,
            .device               = **m_Device,
            .pAllocationCallbacks = &static_cast<VkAllocationCallbacks&>(m_CustomAllocator),
            .instance             = **m_Instance,
            .vulkanApiVersion     = m_Context.enumerateInstanceVersion(),
        };
        vmaCreateAllocator(&allocator_info, &m_VmaAllocator);
    }

    m_Logger->trace("Initialized.");
}

VulkanDevice::~VulkanDevice() {
    m_Device->waitIdle();
    vmaDestroyAllocator(m_VmaAllocator);
}

void VulkanDevice::Tick() {
    Device::Tick();
    if (m_EnableProfile) {
        Profile();
    }
}

void VulkanDevice::Profile() const {
    static bool configured = false;
    if (!configured) {
        TracyPlotConfig("GPU Allocations", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("GPU Memory", tracy::PlotFormatType::Memory, false, true, 0);
        configured = true;
    }

    vmaSetCurrentFrameIndex(m_VmaAllocator, m_FrameIndex);
    VmaTotalStatistics statics;
    vmaCalculateStatistics(m_VmaAllocator, &statics);
    TracyPlot("GPU Allocations", static_cast<std::int64_t>(statics.total.statistics.allocationCount));
    TracyPlot("GPU Memory", static_cast<std::int64_t>(statics.total.statistics.allocationBytes));
}

}  // namespace hitagi::gfx
