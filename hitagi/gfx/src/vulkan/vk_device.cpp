#include "vk_device.hpp"
#include "vk_sync.hpp"
#include "vk_resource.hpp"
#include "vk_command_buffer.hpp"
#include "vk_configs.hpp"
#include "utils.hpp"

#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

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
        const vk::ApplicationInfo app_info{
            .pApplicationName   = "Hitagi",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName        = "Hitagi",
            .engineVersion      = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion         = m_Context.enumerateInstanceVersion(),
        };
        m_Logger->debug("Vulkan API Version: {}.{}.{}",
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

    m_Logger->debug("Enable validation message logger...");
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

    m_Logger->debug("Pick GPU...");
    {
        auto physical_devices = m_Instance->enumeratePhysicalDevices();
        m_Logger->debug("Found {} Vulkan physical devices", physical_devices.size());
        for (const auto& device : physical_devices) {
            m_Logger->debug("\t - {}", device.getProperties().deviceName);
        }

        std::erase_if(physical_devices, [](const auto& device) { return !is_physical_suitable(device); });
        std::ranges::sort(physical_devices, std::ranges::greater(), compute_physical_device_score);
        if (physical_devices.empty()) {
            throw std::runtime_error("Failed to find physical device Vulkan supported!");
        }
        m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(std::move(physical_devices.front()));
        m_Logger->debug("Pick physical device: {}", fmt::styled(std::string_view{m_PhysicalDevice->getProperties().deviceName}, fmt::fg(fmt::color::green)));
    }

    m_Logger->debug("Create logical device...");
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
                .descriptorIndexing                            = true,
                .shaderInputAttachmentArrayDynamicIndexing     = true,
                .shaderInputAttachmentArrayNonUniformIndexing  = true,
                .descriptorBindingUniformBufferUpdateAfterBind = true,
                .descriptorBindingSampledImageUpdateAfterBind  = true,
                .descriptorBindingStorageImageUpdateAfterBind  = true,
                .descriptorBindingStorageBufferUpdateAfterBind = true,
                .descriptorBindingPartiallyBound               = true,
                .descriptorBindingVariableDescriptorCount      = true,
                .timelineSemaphore                             = true,
            },
            vk::PhysicalDeviceVulkan13Features{
                .synchronization2 = true,
                .dynamicRendering = true,
            },
        };

        m_Device = std::make_unique<vk::raii::Device>(m_PhysicalDevice->createDevice(device_create_info.get(), GetCustomAllocator()));

        m_Logger->debug("Retrieval command queues ...");
        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            m_CommandQueues[type] = std::make_unique<VulkanCommandQueue>(
                *this,
                type,
                fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)),
                queue_create_info.queueFamilyIndex);
        });
    }

    m_Logger->debug("Create VMA Allocator...");
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

    m_Logger->debug("Create Command Pools...");
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

    m_Logger->debug("Create Descriptor Set Pools...");
    {
        const auto limits = m_PhysicalDevice->getProperties().limits;

        std::array<vk::DescriptorPoolSize, 5> pool_sizes = {{
            {vk::DescriptorType::eSampler, limits.maxDescriptorSetSamplers},
            {vk::DescriptorType::eSampledImage, limits.maxDescriptorSetSampledImages},
            {vk::DescriptorType::eStorageImage, limits.maxDescriptorSetStorageImages},
            {vk::DescriptorType::eUniformBuffer, limits.maxDescriptorSetUniformBuffersDynamic},
            {vk::DescriptorType::eStorageBuffer, limits.maxDescriptorSetStorageBuffersDynamic},
            // TODO ray tracing
        }};

        const vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
            .flags         = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets       = pool_sizes.size(),  // we will make sure each set only contains one descriptor type
            .poolSizeCount = pool_sizes.size(),
            .pPoolSizes    = pool_sizes.data(),
        };

        m_DescriptorPool = std::make_unique<vk::raii::DescriptorPool>(
            *m_Device,
            descriptor_pool_create_info,
            GetCustomAllocator());

        m_BindlessRootSignature = std::make_shared<VulkanPipelineLayout>(*this, std::span{pool_sizes.data(), pool_sizes.size()});
    }

    m_Logger->debug("Create shader compiler...");
    {
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)))) {
            throw std::runtime_error("Failed to create DXC utils!");
        }
        if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_ShaderCompiler)))) {
            throw std::runtime_error("Failed to create DXC shader compiler!");
        }
    }
}

VulkanDevice::~VulkanDevice() {
    vmaDestroyAllocator(m_VmaAllocator);
}

void VulkanDevice::WaitIdle() {
    m_Device->waitIdle();
}

auto VulkanDevice::CreateFence(std::string_view name) -> std::shared_ptr<Fence> {
    return std::make_shared<VulkanFence>(*this, name);
}

auto VulkanDevice::CreateSemaphore(std::string_view name) -> std::shared_ptr<Semaphore> {
    return std::make_shared<VulkanSemaphore>(*this, name);
}

auto VulkanDevice::GetCommandQueue(CommandType type) const -> CommandQueue& {
    return *m_CommandQueues[type];
}

auto VulkanDevice::CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return std::make_shared<VulkanGraphicsCommandBuffer>(*this, name);
}

auto VulkanDevice::CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return std::make_shared<VulkanComputeCommandBuffer>(*this, name);
}

auto VulkanDevice::CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    return std::make_shared<VulkanTransferCommandBuffer>(*this, name);
}

auto VulkanDevice::CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> {
    return std::make_shared<VulkanSwapChain>(*this, desc);
}

auto VulkanDevice::CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GPUBuffer> {
    return std::make_shared<VulkanBuffer>(*this, desc, initial_data);
}

auto VulkanDevice::CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return std::make_shared<VulkanImage>(*this, desc, initial_data);
}

auto VulkanDevice::CreatSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> {
    return std::make_shared<VulkanSampler>(*this, desc);
}

auto VulkanDevice::CreateShader(ShaderDesc desc, std::span<const std::byte> binary_program) -> std::shared_ptr<Shader> {
    return std::make_shared<VulkanShader>(*this, desc, binary_program);
}

auto VulkanDevice::CreateRootSignature(RootSignatureDesc desc) -> std::shared_ptr<RootSignature> {
    return std::make_shared<VulkanPipelineLayout>(*this, std::move(desc));
}

auto VulkanDevice::CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> {
    return std::make_shared<VulkanRenderPipeline>(*this, std::move(desc));
}

auto VulkanDevice::CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> {
    return std::make_shared<VulkanComputePipeline>(*this, std::move(desc));
}

void VulkanDevice::Profile(std::size_t frame_index) const {}

auto VulkanDevice::CompileShader(const ShaderDesc& desc) const -> core::Buffer {
    std::pmr::vector<std::pmr::wstring> args;

    // shader name
    args.emplace_back(std::pmr::wstring(desc.name.begin(), desc.name.end()));

    // shader entry
    args.emplace_back(L"-E");
    args.emplace_back(std::pmr::wstring(desc.entry.begin(), desc.entry.end()));

    // shader model
    args.emplace_back(L"-T");
    args.emplace_back(get_shader_model_version(desc.type));

    // We use SPIR-V here
    args.emplace_back(L"-spirv");

    // We use row major order
    args.emplace_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);

    // make sure all resource bound
    args.emplace_back(DXC_ARG_ALL_RESOURCES_BOUND);
#ifdef HITAGI_DEBUG
    args.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL0);
    args.emplace_back(DXC_ARG_DEBUG);
    args.emplace_back(L"-Qembed_debug");
#endif
    std::pmr::vector<const wchar_t*> p_args;
    std::transform(args.begin(), args.end(), std::back_insert_iterator(p_args), [](const auto& arg) { return arg.data(); });

    std::pmr::string compile_command;
    for (std::pmr::wstring arg : args) {
        compile_command.append(arg.begin(), arg.end());
        compile_command.push_back(' ');
    }
    m_Logger->debug("Compile Shader: {}", compile_command);

    ComPtr<IDxcIncludeHandler> include_handler;

    m_DxcUtils->CreateDefaultIncludeHandler(&include_handler);
    DxcBuffer source_buffer{
        .Ptr      = desc.source_code.data(),
        .Size     = desc.source_code.size(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<IDxcResult> compile_result;

    if (FAILED(m_ShaderCompiler->Compile(
            &source_buffer,
            p_args.data(),
            p_args.size(),
            include_handler.Get(),
            IID_PPV_ARGS(&compile_result)))) {
        m_Logger->error("Failed to compile shader {} with entry {}", desc.name, desc.entry);
        return {};
    }

    ComPtr<IDxcBlobUtf8> compile_errors;
    compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compile_errors), nullptr);
    // Note that DirectX compiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (compile_errors != nullptr && compile_errors->GetStringLength() != 0) {
        m_Logger->warn("Warnings and Errors:\n{}\n", compile_errors->GetStringPointer());
    }

    HRESULT hr;
    compile_result->GetStatus(&hr);
    if (FAILED(hr)) {
        m_Logger->error("Failed to compile shader {} with entry {}", desc.name, desc.entry);
        return {};
    }

    ComPtr<IDxcBlob>     shader_buffer;
    ComPtr<IDxcBlobWide> shader_name;

    compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_buffer), &shader_name);
    if (shader_buffer == nullptr) {
        return {};
    }

    core::Buffer result{shader_buffer->GetBufferSize(), reinterpret_cast<const std::byte*>(shader_buffer->GetBufferPointer())};

    return result;
}

}  // namespace hitagi::gfx