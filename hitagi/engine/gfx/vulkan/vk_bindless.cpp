module;
#include "interop/tracy_macros.hpp"

export module gfx.vulkan:bindless;
import interop.vma;
import interop.tracy;
import interop.vulkan;

import std;
import gfx.base;
import :types;

export namespace hitagi::gfx {

struct VulkanBindlessUtils final : public BindlessUtils {
    VulkanBindlessUtils(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, VmaAllocator allocator, bool descriptor_heap, std::string_view name);
    ~VulkanBindlessUtils() final;
    auto& GetNativeDevice() const noexcept { return m_Device; }

    auto CreateBindlessHandle(const Sampler& sampler) -> BindlessHandle;
    void DiscardBindlessHandle(BindlessHandle handle) final;

    void Bind(const vk::raii::CommandBuffer& command_buffer, vk::PipelineBindPoint bind_point) const;
    void PushMetaInfo(const vk::raii::CommandBuffer& command_buffer, const BindlessMetaInfo& info) const;
    void ConfigureShader(vk::PipelineShaderStageCreateInfo& info) const;

    auto CreatePipeline(vk::GraphicsPipelineCreateInfo info, const vk::AllocationCallbacks& callbacks) const -> std::unique_ptr<vk::raii::Pipeline>;
    auto CreatePipeline(vk::ComputePipelineCreateInfo info, const vk::AllocationCallbacks& callbacks) const -> std::unique_ptr<vk::raii::Pipeline>;

    auto CreateBindlessHandle(const GPUBufferViewDesc& desc, std::uint64_t size) -> BindlessHandle;
    auto CreateBindlessHandle(const TextureView& view) -> BindlessHandle;

private:
    void Write(std::uint32_t index, const GPUBufferViewDesc& desc, std::uint64_t size);
    void Write(std::uint32_t index, const TextureView& view);
    void Write(std::uint32_t index, const Sampler& sampler);

    const vk::raii::Device& m_Device;

    std::variant<std::unique_ptr<VulkanDescriptorHeap>, std::unique_ptr<VulkanDescriptorIndexing>> m_Backend;

    struct BindlessHandlePool {
        std::pmr::vector<BindlessHandle> pool;
        TracyLockableN(std::mutex, mutex, "Vulkan Bindless Pool Mutex");
    };
    std::array<BindlessHandlePool, 4> m_BindlessHandlePools{};
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

auto VulkanBindlessUtils::CreateBindlessHandle(const GPUBufferViewDesc& desc, std::uint64_t size) -> BindlessHandle {
    auto& [pool, mutex] = m_BindlessHandlePools[0];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless buffer heap exhausted");
    auto handle = pool.back();
    Write(handle.index, desc, size);
    pool.pop_back();
    handle.type     = BindlessHandleType::Buffer;
    handle.writable = desc.type == GPUBufferViewType::StorageWrite;
    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(const TextureView& view) -> BindlessHandle {
    const auto& desc     = view.GetDesc();
    const bool  writable = desc.type == TextureViewType::ShaderWrite;
    const auto  kind     = writable ? 2 : 1;
    auto& [pool, mutex] = m_BindlessHandlePools[kind];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless image heap exhausted");
    auto handle = pool.back();
    Write(handle.index, view);
    pool.pop_back();
    handle.type     = BindlessHandleType::Texture;
    handle.writable = writable;
    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(const Sampler& sampler) -> BindlessHandle {
    auto& [pool, mutex] = m_BindlessHandlePools[3];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless sampler heap exhausted");
    auto handle = pool.back();
    Write(handle.index, sampler);
    pool.pop_back();
    handle.type = BindlessHandleType::Sampler;
    return handle;
}

void VulkanBindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    ZoneScoped;
    if (!handle) return;
    int kind = handle.writable ? 2 : 1;
    if (handle.type == BindlessHandleType::Buffer)
        kind = 0;
    else if (handle.type == BindlessHandleType::Sampler)
        kind = 3;
    auto& [pool, mutex] = m_BindlessHandlePools[kind];
    std::scoped_lock lock{mutex};
    ++handle.version;
    handle.type = BindlessHandleType::Invalid;
    pool.emplace_back(handle);
}

}  // namespace hitagi::gfx
