#include "vk_device.hpp"
#include "vk_resource.hpp"
#include "configs.hpp"
#include "utils.hpp"

#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace hitagi::gfx {
auto custom_vk_allocation_fn(void*, std::size_t, std::size_t, VkSystemAllocationScope) -> void*;
auto custom_vk_reallocation_fn(void*, void*, std::size_t, std::size_t, VkSystemAllocationScope) -> void*;
auto custom_vk_free_fn(void*, void*) -> void;

VulkanDevice::VulkanDevice(std::string_view name)
    : Device(Device::Type::Vulkan, name),
      m_CustomAllocator{
          .pUserData       = this,
          .pfnAllocation   = custom_vk_allocation_fn,
          .pfnReallocation = custom_vk_reallocation_fn,
          .pfnFree         = custom_vk_free_fn,
      }

{
    m_Logger->debug("Create Vulkan Instance...");
    {
        vk::ApplicationInfo app_info = {
            .pApplicationName   = "Hitagi",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName        = "Hitagi",
            .engineVersion      = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion         = m_Context.enumerateInstanceVersion(),
        };

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

    m_Logger->debug("Enable validation message logger...");
    {
        m_DebugUtilsMessager = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(
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
            });
    }

    m_Logger->debug("Pick GPU...");
    {
        auto physical_devices = m_Instance->enumeratePhysicalDevices();
        m_Logger->debug("Found {} Vulkan physical devices", physical_devices.size());
        for (const auto& device : physical_devices) {
            m_Logger->debug("\t {}", device.getProperties().deviceName);
        }

        std::erase_if(physical_devices, [](const auto& device) { return !is_physcial_suitable(device); });
        std::ranges::sort(physical_devices, std::ranges::greater(), compute_physical_device_scroe);
        if (physical_devices.empty()) {
            throw std::runtime_error("Failed to find physical device Vulkan supported!");
        }
        m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(std::move(physical_devices.front()));
    }
    m_Logger->debug("Pick physical device: {}", fmt::styled(std::string_view{m_PhysicalDevice->getProperties().deviceName}, fmt::fg(fmt::color::green)));

    m_Logger->debug("Create logical device...");
    {
        const auto queue_create_info = get_queue_create_info(*m_PhysicalDevice).value();

        m_Device = std::make_unique<vk::raii::Device>(m_PhysicalDevice->createDevice(
            vk::DeviceCreateInfo{
                .queueCreateInfoCount    = 1,
                .pQueueCreateInfos       = &queue_create_info,
                .enabledExtensionCount   = static_cast<std::uint32_t>(required_physical_device_extensions.size()),
                .ppEnabledExtensionNames = required_physical_device_extensions.data(),
            },
            GetCustomAllocator()));

        m_Logger->debug("Retrival command queues ...");
        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            m_CommandQueues[type] = std::make_unique<VulkanCommandQueue>(
                *this,
                type,
                fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)),
                queue_create_info.queueFamilyIndex);
        });
    }
}

void VulkanDevice::WaitIdle() {
    m_Device->waitIdle();
}

auto VulkanDevice::GetCommandQueue(CommandType type) const -> CommandQueue& {
    return *m_CommandQueues[type];
}

auto VulkanDevice::CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return nullptr;
}

auto VulkanDevice::CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return nullptr;
}

auto VulkanDevice::CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    return nullptr;
}

auto VulkanDevice::CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain> {
    return std::make_shared<VulkanSwapChain>(*this, desc);
}

auto VulkanDevice::CreateGpuBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GpuBuffer> {
    return std::make_shared<VulkanBuffer>(*this, desc);
}

auto VulkanDevice::CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return nullptr;
}

auto VulkanDevice::CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler> {
    return nullptr;
}

void VulkanDevice::CompileShader(Shader& shader) {}

auto VulkanDevice::CreateRenderPipeline(RenderPipeline::Desc desc) -> std::shared_ptr<RenderPipeline> {
    return nullptr;
}

void VulkanDevice::Profile(std::size_t frame_index) const {}

}  // namespace hitagi::gfx