#include "vulkan_device.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::gfx {

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
    vk::raii::Context context;

    vk::ApplicationInfo app_info = {
        "Hitagi",
        VK_MAKE_VERSION(0, 0, 1),
        "Hitagi",
        VK_MAKE_VERSION(0, 0, 1),
        context.enumerateInstanceVersion(),
    };

    m_Instance = std::make_unique<vk::raii::Instance>(
        context,
        vk::InstanceCreateInfo{
            vk::InstanceCreateFlags(),
            &app_info,
            0,
            nullptr,
            0,
            nullptr,
        },
        m_CustomAllocationCallback);

    auto physical_devices = m_Instance->enumeratePhysicalDevices();

    m_Device = std::make_unique<vk::raii::Device>(
        physical_devices[0],
        vk::DeviceCreateInfo{
            vk::DeviceCreateFlags(),
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr,
        },
        m_CustomAllocationCallback);
}

void VulkanDevice::WaitIdle() {}

auto VulkanDevice::GetCommandQueue(CommandType type) const -> CommandQueue* {
    return nullptr;
}

auto VulkanDevice::CreateCommandQueue(CommandType type, std::string_view name) -> std::shared_ptr<CommandQueue> {
    return nullptr;
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

}  // namespace hitagi::gfx