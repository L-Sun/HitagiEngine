#pragma once
#include "vk_command_queue.hpp"

#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/command_context.hpp>
#include <hitagi/utils/array.hpp>

#include <dxc/dxcapi.h>
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

namespace hitagi::gfx {
class VulkanDevice final : public Device {
public:
    VulkanDevice(std::string_view name);
    ~VulkanDevice() final;

    void WaitIdle() final;

    auto CreateSemaphore(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Semaphore> final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> final;
    auto CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> final;
    auto CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> final;

    auto CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBuffer::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(Shader::Desc desc, std::span<const std::byte> binary_program = {}) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(GraphicsPipeline::Desc desc) -> std::shared_ptr<GraphicsPipeline> final;

    void Profile(std::size_t frame_index) const final;

    inline auto& GetInstance() const noexcept { return *m_Instance; }
    inline auto& GetCustomAllocator() const noexcept { return m_CustomAllocator; }
    inline auto& GetCustomAllocationRecord() noexcept { return m_CustomAllocationRecord; }
    inline auto& GetPhysicalDevice() const noexcept { return *m_PhysicalDevice; }
    inline auto& GetDevice() const noexcept { return *m_Device; }
    inline auto& GetCommandPool(CommandType type) const noexcept { return *m_CommandPools[type]; }
    inline auto& GetVkCommandQueue(CommandType type) const noexcept { return *m_CommandQueues[type]; }
    inline auto& GetVmaAllocator() const noexcept { return m_VmaAllocator; }

private:
    vk::AllocationCallbacks m_CustomAllocator;
    using AllocationRecord = std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>>;
    AllocationRecord m_CustomAllocationRecord;

    vk::raii::Context                                 m_Context;
    std::unique_ptr<vk::raii::Instance>               m_Instance;
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugUtilsMessenger;

    std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice;
    std::unique_ptr<vk::raii::Device>         m_Device;

    utils::EnumArray<std::unique_ptr<vk::raii::CommandPool>, CommandType> m_CommandPools;
    utils::EnumArray<std::unique_ptr<VulkanCommandQueue>, CommandType>    m_CommandQueues;

    VmaAllocator m_VmaAllocator;

    CComPtr<IDxcUtils>     m_DxcUtils;
    CComPtr<IDxcCompiler3> m_ShaderCompiler;
};
}  // namespace hitagi::gfx