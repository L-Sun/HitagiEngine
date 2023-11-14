#include "vk_device.hpp"
#include "vk_sync.hpp"
#include "vk_resource.hpp"
#include "vk_command_buffer.hpp"
#include "vk_configs.hpp"
#include "vk_utils.hpp"

#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

namespace hitagi::gfx {
VulkanDevice::VulkanDevice(std::string_view name)
    : Device(Device::Type::Vulkan, name),
      m_CustomAllocator{
          .pUserData       = this,
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
            throw std::runtime_error("Failed to find physical device Vulkan supported!");
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
                // -- Bindless features
                .descriptorIndexing                            = true,
                .shaderInputAttachmentArrayDynamicIndexing     = true,
                .shaderInputAttachmentArrayNonUniformIndexing  = true,
                .descriptorBindingUniformBufferUpdateAfterBind = true,
                .descriptorBindingSampledImageUpdateAfterBind  = true,
                .descriptorBindingStorageImageUpdateAfterBind  = true,
                .descriptorBindingStorageBufferUpdateAfterBind = true,
                .descriptorBindingPartiallyBound               = true,
                .descriptorBindingVariableDescriptorCount      = true,
                .runtimeDescriptorArray                        = true,
                // -- Bindless features
                .timelineSemaphore = true,
            },
            vk::PhysicalDeviceVulkan13Features{
                .synchronization2 = true,
                .dynamicRendering = true,
            },
        };

        m_Device = std::make_unique<vk::raii::Device>(m_PhysicalDevice->createDevice(device_create_info.get(), GetCustomAllocator()));

        m_Logger->trace("Retrieval command queues ...");
        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            m_CommandQueues[type] = std::make_unique<VulkanCommandQueue>(
                *this,
                type,
                fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)),
                queue_create_info.queueFamilyIndex);
        });
    }

    m_Logger->trace("Create VMA Allocator...");
    {
        const VmaAllocatorCreateInfo allocator_info = {
            .physicalDevice       = **m_PhysicalDevice,
            .device               = **m_Device,
            .pAllocationCallbacks = &static_cast<VkAllocationCallbacks&>(m_CustomAllocator),
            .instance             = **m_Instance,
            .vulkanApiVersion     = m_Context.enumerateInstanceVersion(),
        };
        vmaCreateAllocator(&allocator_info, &m_VmaAllocator);
    }

    m_Logger->trace("Create Command Pools...");
    {
        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            vk::CommandPoolCreateInfo command_pool_create_info{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                         vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = m_CommandQueues[type]->GetFamilyIndex(),
            };
            m_CommandPools[type] = std::make_unique<vk::raii::CommandPool>(
                *m_Device,
                command_pool_create_info,
                GetCustomAllocator());
        });
    }

    m_Logger->trace("Create Bindless...");
    {
        m_BindlessUtils = std::make_unique<VulkanBindlessUtils>(*this, fmt::format("{}-BindlessUtils", m_Name));
    }
    m_Logger->trace("Initialized.");
}

VulkanDevice::~VulkanDevice() {
    vmaDestroyAllocator(m_VmaAllocator);
}

void VulkanDevice::Tick() {
    Device::Tick();
    if (m_EnableProfile) {
        Profile();
    }
}

void VulkanDevice::WaitIdle() {
    m_Device->waitIdle();
}

auto VulkanDevice::CreateFence(std::uint64_t initial_value, std::string_view name) -> std::shared_ptr<Fence> {
    return std::make_shared<VulkanTimelineSemaphore>(*this, initial_value, name);
}

auto VulkanDevice::GetCommandQueue(CommandType type) const -> CommandQueue& {
    return *m_CommandQueues[type];
}

auto VulkanDevice::CreateCommandContext(CommandType type, std::string_view name) -> std::shared_ptr<CommandContext> {
    switch (type) {
        case CommandType::Graphics:
            return std::make_shared<VulkanGraphicsCommandBuffer>(*this, name);
        case CommandType::Compute:
            return std::make_shared<VulkanComputeCommandBuffer>(*this, name);
        case CommandType::Copy:
            return std::make_shared<VulkanTransferCommandBuffer>(*this, name);
        default:
            throw std::runtime_error("Invalid command type.");
    }
}

auto VulkanDevice::CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> {
    return std::make_shared<VulkanSwapChain>(*this, std::move(desc));
}

auto VulkanDevice::CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GPUBuffer> {
    return std::make_shared<VulkanBuffer>(*this, std::move(desc), initial_data);
}

auto VulkanDevice::CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return std::make_shared<VulkanImage>(*this, std::move(desc), initial_data);
}

auto VulkanDevice::CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> {
    return std::make_shared<VulkanSampler>(*this, std::move(desc));
}

auto VulkanDevice::CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> {
    return std::make_shared<VulkanShader>(*this, std::move(desc));
}

auto VulkanDevice::CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> {
    return std::make_shared<VulkanRenderPipeline>(*this, std::move(desc));
}

auto VulkanDevice::CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> {
    return std::make_shared<VulkanComputePipeline>(*this, std::move(desc));
}

auto VulkanDevice::GetBindlessUtils() -> BindlessUtils& {
    return *m_BindlessUtils;
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