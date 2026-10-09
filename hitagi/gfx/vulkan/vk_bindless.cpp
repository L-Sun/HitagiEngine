module;
#include "interop/tracy_macros.hpp"

export module gfx.vulkan:bindless;
import interop.vma;
import interop.tracy;
import interop.vulkan;
import interop.magic_enum;

import std;
import core;
import utils;
import math;
import gfx.base;
import :utils;
import :configs;

export namespace hitagi::gfx {

struct VulkanBindlessUtils : public BindlessUtils {
    VulkanBindlessUtils(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, VmaAllocator allocator, std::string_view name);
    auto& GetNativeDevice() const noexcept { return m_Device; }

    auto CreateBindlessHandle(const SamplerDesc& desc) -> BindlessHandle;
    void DiscardBindlessHandle(BindlessHandle handle) final;

    void Bind(const vk::raii::CommandBuffer& command_buffer) const;

    vk::ShaderDescriptorSetAndBindingMappingInfoEXT mapping_info;

public:
    auto CreateBindlessHandle(vk::DeviceAddress address, const GPUBufferViewDesc& desc, std::uint64_t size) -> BindlessHandle;
    auto CreateBindlessHandle(vk::Image image, const TextureViewDesc& desc) -> BindlessHandle;

private:
    const vk::raii::Device& m_Device;

    struct Heap {
        ~Heap();
        VmaAllocator        allocator  = nullptr;
        VmaAllocation       allocation = nullptr;
        VkBuffer            buffer     = hitagi::interop::null_handle;
        std::byte*          mapped     = nullptr;
        vk::BindHeapInfoEXT bind_info;
    };
    std::array<Heap, 2>                                  m_Heaps;
    std::array<vk::DescriptorSetAndBindingMappingEXT, 4> m_Mappings;
    std::array<std::uint32_t, 4>                         m_DescriptorSizes;
    std::array<std::uint32_t, 4>                         m_Offsets;

    struct BindlessHandlePool {
        std::pmr::vector<BindlessHandle> pool;
        TracyLockableN(std::mutex, mutex, "Vulkan Bindless Pool Mutex");
    };
    std::array<BindlessHandlePool, 4> m_BindlessHandlePools{};
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

VulkanBindlessUtils::Heap::~Heap() {
    if (buffer) vmaDestroyBuffer(allocator, buffer, allocation);
}

VulkanBindlessUtils::VulkanBindlessUtils(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, VmaAllocator allocator, std::string_view name)
    : BindlessUtils(name), m_Device(device) {
    const auto  properties = physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
    const auto& limits     = properties.get<vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
    if (limits.maxPushDataSize < sizeof(BindlessMetaInfo)) throw std::runtime_error("Insufficient Vulkan push data size");

    m_DescriptorSizes                        = {static_cast<std::uint32_t>(limits.bufferDescriptorSize), static_cast<std::uint32_t>(limits.imageDescriptorSize), static_cast<std::uint32_t>(limits.imageDescriptorSize), static_cast<std::uint32_t>(limits.samplerDescriptorSize)};
    const std::array              counts     = {max_storage_descriptors, max_sampled_image_descriptors, max_storage_image_descriptors, max_sampler_descriptors};
    const std::array              alignments = {limits.bufferDescriptorAlignment, limits.imageDescriptorAlignment, limits.imageDescriptorAlignment, limits.samplerDescriptorAlignment};
    std::array<vk::DeviceSize, 2> sizes{};
    for (std::uint32_t kind = 0; kind < counts.size(); ++kind) {
        auto& size              = sizes[kind == 3 ? 1 : 0];
        size                    = utils::align(size, alignments[kind]);
        m_Offsets[kind]         = static_cast<std::uint32_t>(size);
        m_DescriptorSizes[kind] = utils::align(m_DescriptorSizes[kind], alignments[kind]);
        size += vk::DeviceSize(m_DescriptorSizes[kind]) * counts[kind];
        m_Mappings[kind] = {
            .descriptorSet = kind,
            .firstBinding  = 0,
            .bindingCount  = 1,
            .resourceMask  = vk::SpirvResourceTypeFlagBitsEXT::eAll,
            .source        = vk::DescriptorMappingSourceEXT::eHeapWithConstantOffset,
        };
        m_Mappings[kind].sourceData.constantOffset = {.heapOffset = m_Offsets[kind], .heapArrayStride = m_DescriptorSizes[kind]};
        auto& pool                                 = m_BindlessHandlePools[kind].pool;
        pool.reserve(counts[kind]);
        for (std::uint32_t index = 0; index < counts[kind]; ++index) pool.emplace_back(BindlessHandle{.index = index});
    }
    mapping_info = {.mappingCount = static_cast<std::uint32_t>(m_Mappings.size()), .pMappings = m_Mappings.data()};

    const std::array heap_alignments     = {limits.resourceHeapAlignment, limits.samplerHeapAlignment};
    const std::array reserved_alignments = {std::max(limits.bufferDescriptorAlignment, limits.imageDescriptorAlignment), limits.samplerDescriptorAlignment};
    const std::array reserved_sizes      = {limits.minResourceHeapReservedRange, limits.minSamplerHeapReservedRange};
    const std::array max_sizes           = {limits.maxResourceHeapSize, limits.maxSamplerHeapSize};
    for (std::size_t index = 0; index < m_Heaps.size(); ++index) {
        auto& heap                         = m_Heaps[index];
        heap.allocator                     = allocator;
        heap.bind_info.reservedRangeOffset = utils::align(sizes[index], reserved_alignments[index]);
        heap.bind_info.reservedRangeSize   = utils::align(reserved_sizes[index], reserved_alignments[index]);
        const auto size                    = heap.bind_info.reservedRangeOffset + heap.bind_info.reservedRangeSize;
        if (size > max_sizes[index]) throw std::runtime_error("Vulkan descriptor heap capacity exceeds device limits");
        const VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        };
        const VmaAllocationCreateInfo allocation_info{
            .flags         = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage         = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        VmaAllocationInfo allocation{};
        if (vmaCreateBufferWithAlignment(heap.allocator, &buffer_info, &allocation_info, heap_alignments[index], &heap.buffer, &heap.allocation, &allocation) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Vulkan descriptor heap");
        }
        heap.mapped              = static_cast<std::byte*>(allocation.pMappedData);
        heap.bind_info.heapRange = {.address = device.getBufferAddress({.buffer = heap.buffer}), .size = size};
    }
}

void VulkanBindlessUtils::Bind(const vk::raii::CommandBuffer& command_buffer) const {
    command_buffer.bindResourceHeapEXT(m_Heaps[0].bind_info);
    command_buffer.bindSamplerHeapEXT(m_Heaps[1].bind_info);
}

auto VulkanBindlessUtils::CreateBindlessHandle(vk::DeviceAddress address, const GPUBufferViewDesc& desc, std::uint64_t size) -> BindlessHandle {
    const auto& device  = m_Device;
    auto& [pool, mutex] = m_BindlessHandlePools[0];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless buffer heap exhausted");
    auto handle = pool.back();
    const vk::DeviceAddressRangeEXT range{
        .address = address + desc.offset,
        .size    = size,
    };
    vk::ResourceDescriptorInfoEXT resource{.type = vk::DescriptorType::eStorageBuffer};
    resource.data.pAddressRange = &range;
    device.writeResourceDescriptorsEXT(resource, vk::HostAddressRangeEXT{
                                                     .address = m_Heaps[0].mapped + m_Offsets[0] + handle.index * m_DescriptorSizes[0], .size = m_DescriptorSizes[0]});
    pool.pop_back();
    handle.type     = BindlessHandleType::Buffer;
    handle.writable = desc.type == GPUBufferViewType::StorageWrite;
    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(vk::Image image, const TextureViewDesc& desc) -> BindlessHandle {
    const bool  writable = desc.type == TextureViewType::ShaderWrite;
    const auto  kind     = writable ? 2 : 1;
    const auto& device   = m_Device;
    auto& [pool, mutex]  = m_BindlessHandlePools[kind];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless image heap exhausted");
    auto                             handle    = pool.back();
    const auto                       view_info = to_vk_image_view_create_info(desc, image);
    const vk::ImageDescriptorInfoEXT image_info{.pView = &view_info, .layout = writable ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::ResourceDescriptorInfoEXT    resource{.type = writable ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage};
    resource.data.pImage = &image_info;
    device.writeResourceDescriptorsEXT(resource, vk::HostAddressRangeEXT{
                                                     .address = m_Heaps[0].mapped + m_Offsets[kind] + handle.index * m_DescriptorSizes[kind], .size = m_DescriptorSizes[kind]});
    pool.pop_back();
    handle.type     = BindlessHandleType::Texture;
    handle.writable = writable;
    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(const SamplerDesc& desc) -> BindlessHandle {
    const auto& device  = m_Device;
    auto& [pool, mutex] = m_BindlessHandlePools[3];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless sampler heap exhausted");
    auto handle = pool.back();
    device.writeSamplerDescriptorsEXT(to_vk_sampler_create_info(desc), vk::HostAddressRangeEXT{
                                                                           .address = m_Heaps[1].mapped + handle.index * m_DescriptorSizes[3], .size = m_DescriptorSizes[3]});
    pool.pop_back();
    handle.type = BindlessHandleType::Sampler;
    return handle;
}

void VulkanBindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    ZoneScoped;
    if (!handle) return;
    const auto kind     = handle.type == BindlessHandleType::Buffer ? 0 : handle.type == BindlessHandleType::Sampler ? 3
                                                                      : handle.writable                              ? 2
                                                                                                                     : 1;
    auto& [pool, mutex] = m_BindlessHandlePools[kind];
    std::scoped_lock lock{mutex};
    ++handle.version;
    handle.type = BindlessHandleType::Invalid;
    pool.emplace_back(handle);
}

}  // namespace hitagi::gfx
