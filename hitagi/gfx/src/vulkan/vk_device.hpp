#pragma once
#include "vk_command_queue.hpp"
#include "vk_bindless.hpp"
#undef CreateSemaphore

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

    void WaitIdle() final;

    auto CreateFence(std::string_view name = "") -> std::shared_ptr<Fence> final;
    auto CreateSemaphore(std::string_view name = "") -> std::shared_ptr<Semaphore> final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> final;
    auto CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> final;
    auto CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> final;

    auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreatSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(ShaderDesc desc, std::span<const std::byte> binary_program = {}) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> final;
    auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> final;

    void Profile(std::size_t frame_index) const final;

    inline auto& GetInstance() const noexcept { return *m_Instance; }
    inline auto& GetCustomAllocator() const noexcept { return m_CustomAllocator; }
    inline auto& GetCustomAllocationRecord() noexcept { return m_CustomAllocationRecord; }
    inline auto& GetPhysicalDevice() const noexcept { return *m_PhysicalDevice; }
    inline auto& GetDevice() const noexcept { return *m_Device; }
    inline auto& GetCommandPool(CommandType type) const noexcept { return *m_CommandPools[type]; }
    inline auto& GetVkCommandQueue(CommandType type) const noexcept { return *m_CommandQueues[type]; }
    inline auto& GetVmaAllocator() const noexcept { return m_VmaAllocator; }
    inline auto& GetBindlessUtils() const noexcept { return m_BindlessUtils; }
    inline auto& GetShaderCompiler() const noexcept { return m_ShaderCompiler; }

private:
    vk::AllocationCallbacks m_CustomAllocator;
    using AllocationRecord = std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>>;
    AllocationRecord m_CustomAllocationRecord;

    vk::raii::Context                                 m_Context;
    std::unique_ptr<vk::raii::Instance>               m_Instance;
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugUtilsMessenger;

    std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice;
    std::unique_ptr<vk::raii::Device>         m_Device;

    VmaAllocator m_VmaAllocator;

    utils::EnumArray<std::unique_ptr<vk::raii::CommandPool>, CommandType> m_CommandPools;
    utils::EnumArray<std::unique_ptr<VulkanCommandQueue>, CommandType>    m_CommandQueues;

    VulkanBindlessUtils m_BindlessUtils;

    ShaderCompiler m_ShaderCompiler;
};
}  // namespace hitagi::gfx