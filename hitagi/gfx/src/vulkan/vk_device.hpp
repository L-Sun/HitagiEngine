#pragma once
#include "vk_command_queue.hpp"
#include "vk_resource.hpp"

#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/command_context.hpp>
#include <hitagi/utils/array.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

namespace hitagi::gfx {
class VulkanDevice final : public Device {
public:
    VulkanDevice(std::string_view name);
    ~VulkanDevice() final = default;

    void WaitIdle() final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> final;
    auto CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> final;
    auto CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> final;

    auto CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GpuBuffer> final;
    auto CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler> final;

    void CompileShader(Shader& shader) final;
    auto CreateRenderPipeline(RenderPipeline::Desc desc) -> std::shared_ptr<RenderPipeline> final;

    void Profile(std::size_t frame_index) const final;

    inline auto GetLogger() const noexcept {
        return m_Logger;
    }

    inline auto& GetInstance() const noexcept { return *m_Instance; }
    inline auto& GetCustomAllocator() const noexcept { return m_CustomAllocator; }
    inline auto& GetCustomAllocationRecord() noexcept { return m_CustomAllocationRecord; }
    inline auto& GetPhysicalDevice() const noexcept { return *m_PhysicalDevice; }
    inline auto& GetDevice() const noexcept { return *m_Device; }

private:
    vk::AllocationCallbacks m_CustomAllocator;
    using AllocationRecord = std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>>;
    AllocationRecord m_CustomAllocationRecord;

    vk::raii::Context                                 m_Context;
    std::unique_ptr<vk::raii::Instance>               m_Instance;
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugUtilsMessager;

    std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice;
    std::unique_ptr<vk::raii::Device>         m_Device;

    utils::EnumArray<std::unique_ptr<VulkanCommandQueue>, CommandType> m_CommandQueues;
};
}  // namespace hitagi::gfx