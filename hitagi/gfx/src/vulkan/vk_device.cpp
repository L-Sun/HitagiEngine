#include "vk_device.hpp"
#include "configs.hpp"
#include "utils.hpp"

#include <hitagi/math/transform.hpp>

#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace hitagi::gfx {

VKAPI_ATTR VkBool32 VKAPI_CALL custom_debug_message_fn(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const*, void*);

VulkanDevice::VulkanDevice(std::string_view name)
    : Device(Device::Type::Vulkan, name),
      m_CustomAllocationCallback{
          this,
          [](void* p_this, std::size_t size, std::size_t alignment, VkSystemAllocationScope) {
              return static_cast<VulkanDevice*>(p_this)->CustomAllocateFn(size, alignment);
          },
          [](void* p_this, void* orign_ptr, std::size_t new_size, std::size_t alignment, VkSystemAllocationScope) {
              return static_cast<VulkanDevice*>(p_this)->CustomReallocateFn(orign_ptr, new_size, alignment);
          },
          [](void* p_this, void* ptr) {
              static_cast<VulkanDevice*>(p_this)->CustomFreeFn(ptr);
          },
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

        const auto layers     = GetInstanceLayers();
        const auto extensions = GetInstanceExtensions();

        m_Instance = std::make_unique<vk::raii::Instance>(
            m_Context,
            vk::InstanceCreateInfo{
                .pApplicationInfo        = &app_info,
                .enabledLayerCount       = static_cast<std::uint32_t>(layers.size()),
                .ppEnabledLayerNames     = layers.data(),
                .enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size()),
                .ppEnabledExtensionNames = extensions.data(),
            },
            m_CustomAllocationCallback);
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
        if (physical_devices.empty()) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }
        auto best_device = std::max_element(physical_devices.begin(), physical_devices.end(), [](const auto& lhs, const auto& rhs) {
            return compute_physical_device_scroe(lhs) < compute_physical_device_scroe(rhs);
        });
        m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(*best_device);
    }
    m_Logger->debug("Pick GPU: {}", fmt::styled(std::string_view{m_PhysicalDevice->getProperties().deviceName}, fmt::fg(fmt::color::green)));

    m_Logger->debug("Create logical device...");
    {
        const float queue_priority     = 1.0;
        const auto  queue_create_infos = get_queue_create_infos(*m_PhysicalDevice, queue_priority);

        m_Device = std::make_unique<vk::raii::Device>(m_PhysicalDevice->createDevice(
            vk::DeviceCreateInfo{
                .queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
                .pQueueCreateInfos    = queue_create_infos.data(),
            },
            m_CustomAllocationCallback));

        m_Logger->debug("Retrival command queues ...");
        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            auto queue_family_index = queue_create_infos[magic_enum::enum_integer(type)].queueFamilyIndex;
            m_CommandQueues[type]   = std::make_unique<VulkanCommandQueue>(
                *this,
                type,
                fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)),
                queue_family_index);
        });
    }
}

void VulkanDevice::WaitIdle() {}

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
    return nullptr;
}

auto VulkanDevice::CreateBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GpuBuffer> {
    return nullptr;
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

void* VulkanDevice::CustomAllocateFn(std::size_t size, std::size_t alignment) {
    auto allocator = std::pmr::get_default_resource();
    auto ptr       = allocator->allocate(size, alignment);
    m_CustomAllocationInfos.emplace(ptr, std::make_pair(size, alignment));
    return ptr;
}

void* VulkanDevice::CustomReallocateFn(void* orign_ptr, std::size_t new_size, std::size_t alignment) {
    auto& allocation_infos = m_CustomAllocationInfos;
    auto  allocator        = std::pmr::get_default_resource();
    // just like allocation
    if (orign_ptr == nullptr) {
        auto new_ptr = allocator->allocate(new_size, alignment);
        allocation_infos.emplace(new_ptr, std::make_pair(new_size, alignment));
        return new_ptr;
    }
    // just like free
    else if (new_size == 0) {
        auto [old_size, old_alignment] = allocation_infos.at(orign_ptr);
        allocation_infos.erase(orign_ptr);
        allocator->deallocate(orign_ptr, old_size);
        return nullptr;
    }
    // reallocation
    else {
        // allocate new memory
        auto new_ptr = allocator->allocate(new_size, alignment);
        allocation_infos.emplace(new_ptr, std::make_pair(new_size, alignment));

        auto [old_size, old_alignment] = allocation_infos.at(orign_ptr);
        allocation_infos.erase(orign_ptr);

        // copy origin data to new one
        std::memcpy(new_ptr, orign_ptr, std::min(old_size, new_size));

        // free origin data
        allocator->deallocate(orign_ptr, old_size);

        return new_ptr;
    }
}

void VulkanDevice::CustomFreeFn(void* ptr) {
    if (ptr == nullptr) return;

    auto allocator         = std::pmr::get_default_resource();
    auto [size, alignment] = m_CustomAllocationInfos.at(ptr);
    m_CustomAllocationInfos.erase(ptr);
    allocator->deallocate(ptr, size);
}

auto VulkanDevice::GetInstanceLayers() const -> std::pmr::vector<const char*> {
    auto avaliable_layers = m_Context.enumerateInstanceLayerProperties();

    std::pmr::vector<const char*> enabled_layers;
    std::ranges::copy_if(
        try_enable_instance_layers,
        std::back_inserter(enabled_layers),
        [&avaliable_layers, this](const char* layer_name) {
            if (std::ranges::find_if(avaliable_layers, [layer_name](const auto& layer) {
                    return std::strcmp(layer.layerName, layer_name) == 0;
                }) != avaliable_layers.end()) {
                return true;
            } else {
                m_Logger->warn(
                    "Layer {} is not supported",
                    fmt::styled(layer_name, fmt::fg(fmt::color::orange)));
                return false;
            }
        });

    return enabled_layers;
}

auto VulkanDevice::GetInstanceExtensions() const -> std::pmr::vector<const char*> {
    auto avaliable_extensions = m_Context.enumerateInstanceExtensionProperties();

    std::pmr::vector<const char*> enabled_extensions;
    std::ranges::copy_if(
        try_enable_instance_extensions,
        std::back_inserter(enabled_extensions),
        [&avaliable_extensions, this](const char* extension_name) {
            if (std::ranges::find_if(avaliable_extensions, [extension_name](const auto& extension) {
                    return std::strcmp(extension.extensionName, extension_name) == 0;
                }) != avaliable_extensions.end()) {
                return true;
            } else {
                m_Logger->warn(
                    "Extension {} is not supported",
                    fmt::styled(extension_name, fmt::fg(fmt::color::orange)));
                return false;
            }
        });

    return enabled_extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL custom_debug_message_fn(VkDebugUtilsMessageSeverityFlagBitsEXT _severity, VkDebugUtilsMessageTypeFlagsEXT _type, VkDebugUtilsMessengerCallbackDataEXT const* p_data, void* p_logger) {
    auto logger    = static_cast<spdlog::logger*>(p_logger);
    auto serverity = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(_severity);
    auto type      = static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(_type);

    std::pmr::vector<std::pmr::string> messages;
    messages.emplace_back(fmt::format(
        "{}[{}]:{} -------------",
        fmt::styled(vk::to_string(type), fmt::fg(type == vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance ? fmt::color::orange : fmt::color::red)),
        p_data->pMessageIdName,
        p_data->messageIdNumber));

    messages.emplace_back(p_data->pMessage);

    if (p_data->queueLabelCount > 0) {
        std::pmr::string queue_labels;
        for (std::uint32_t i = 0; i < p_data->queueLabelCount; i++) {
            auto color = math::to_hex(p_data->pQueueLabels->color);
            queue_labels += fmt::format("{}, ", fmt::styled(p_data->pQueueLabels[i].pLabelName, fmt::fg(fmt::rgb(color.r, color.g, color.b))));
            // remove last ", "
            if (i == p_data->queueLabelCount - 1) {
                queue_labels.pop_back();
                queue_labels.pop_back();
            }
        }
        messages.emplace_back(fmt::format("Queue Labels: {}", queue_labels));
    }

    if (p_data->cmdBufLabelCount > 0) {
        std::pmr::string cmd_buf_labels;
        for (std::uint32_t i = 0; i < p_data->cmdBufLabelCount; i++) {
            auto color = math::to_hex(p_data->pCmdBufLabels->color);
            cmd_buf_labels += fmt::format("{}, ", fmt::styled(p_data->pCmdBufLabels[i].pLabelName, fmt::fg(fmt::rgb(color.r, color.g, color.b))));
            // remove last ", "
            if (i == p_data->cmdBufLabelCount - 1) {
                cmd_buf_labels.pop_back();
                cmd_buf_labels.pop_back();
            }
        }
        messages.emplace_back(fmt::format("Command Buffer Labels: {}", cmd_buf_labels));
    }

    if (p_data->objectCount > 0) {
        std::pmr::string objects;
        for (std::uint32_t i = 0; i < p_data->objectCount; i++) {
            objects += fmt::format(
                "{}({}, {}), ",
                p_data->pObjects->pObjectName == nullptr ? "" : p_data->pObjects->pObjectName,
                vk::to_string(static_cast<vk::ObjectType>(p_data->pObjects[i].objectType)),
                p_data->pObjects[i].objectHandle);
            // remove last ", "
            if (i == p_data->objectCount - 1) {
                objects.pop_back();
                objects.pop_back();
            }
        }
        messages.emplace_back(fmt::format("Objects: {}", objects));
    }

    messages.emplace_back(messages.front() + "End");

    for (const auto& message : messages) {
        switch (serverity) {
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                logger->trace(message);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                logger->info(message);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                logger->warn(message);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                logger->error(message);
                break;
            default:
                break;
        }
    }
    return false;
}

}  // namespace hitagi::gfx