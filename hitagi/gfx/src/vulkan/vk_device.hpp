#pragma once
#include "vk_command_queue.hpp"
#include "vk_bindless.hpp"

#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/command_context.hpp>
#include <hitagi/gfx/shader_compiler.hpp>
#include <hitagi/utils/array.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

namespace hitagi::gfx {
class VulkanDevice final : public Device {
public:
    VulkanDevice(std::string_view name);
    ~VulkanDevice() final;

    void Tick() final;

    void WaitIdle() final;

    auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> final;

    auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> final;
    auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> final;

    auto GetBindlessUtils() -> BindlessUtils& final;

    inline auto& GetInstance() const noexcept { return *m_Instance; }
    inline auto& GetCustomAllocator() const noexcept { return m_CustomAllocator; }
    inline auto& GetCustomAllocationRecord() noexcept { return m_CustomAllocationRecord; }
    inline auto& GetPhysicalDevice() const noexcept { return *m_PhysicalDevice; }
    inline auto& GetDevice() const noexcept { return *m_Device; }
    inline auto& GetCommandPool(CommandType type) const noexcept { return *m_CommandPools[type]; }
    inline auto& GetVkCommandQueue(CommandType type) const noexcept { return *m_CommandQueues[type]; }
    inline auto& GetVmaAllocator() const noexcept { return m_VmaAllocator; }
    inline auto& GetBindlessUtils() const noexcept { return m_BindlessUtils; }

private:
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

    VmaAllocator m_VmaAllocator;

    utils::EnumArray<std::unique_ptr<vk::raii::CommandPool>, CommandType> m_CommandPools;
    utils::EnumArray<std::unique_ptr<VulkanCommandQueue>, CommandType>    m_CommandQueues;

    std::unique_ptr<VulkanBindlessUtils> m_BindlessUtils;
};
}  // namespace hitagi::gfx