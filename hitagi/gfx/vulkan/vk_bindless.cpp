module;
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

module gfx.vulkan;
import std;

namespace hitagi::gfx {

VulkanBindlessUtils::Heap::~Heap() {
    if (buffer) vmaDestroyBuffer(allocator, buffer, allocation);
}

VulkanBindlessUtils::VulkanBindlessUtils(VulkanDevice& device, std::string_view name)
    : BindlessUtils(device, name) {
    const auto  properties = device.GetPhysicalDevice().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorHeapPropertiesEXT>();
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
        heap.allocator                     = device.GetVmaAllocator();
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
        heap.bind_info.heapRange = {.address = device.GetDevice().getBufferAddress({.buffer = heap.buffer}), .size = size};
    }
}

void VulkanBindlessUtils::Bind(const vk::raii::CommandBuffer& command_buffer) const {
    command_buffer.bindResourceHeapEXT(m_Heaps[0].bind_info);
    command_buffer.bindSamplerHeapEXT(m_Heaps[1].bind_info);
}

auto VulkanBindlessUtils::CreateBindlessHandle(GPUBufferView& view) -> BindlessHandle {
    const auto& desc    = view.GetDesc();
    auto&       device  = static_cast<VulkanDevice&>(m_Device);
    auto& [pool, mutex] = m_BindlessHandlePools[0];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless buffer heap exhausted");
    auto handle = pool.back();
    const vk::DeviceAddressRangeEXT range{
        .address = device.GetDevice().getBufferAddress({.buffer = **static_cast<VulkanBuffer&>(*desc.buffer).buffer}) + desc.offset,
        .size    = view.Size(),
    };
    vk::ResourceDescriptorInfoEXT resource{.type = vk::DescriptorType::eStorageBuffer};
    resource.data.pAddressRange = &range;
    device.GetDevice().writeResourceDescriptorsEXT(resource, vk::HostAddressRangeEXT{
                                                                 .address = m_Heaps[0].mapped + m_Offsets[0] + handle.index * m_DescriptorSizes[0], .size = m_DescriptorSizes[0]});
    pool.pop_back();
    handle.type     = BindlessHandleType::Buffer;
    handle.writable = desc.type == GPUBufferViewType::StorageWrite;
    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(TextureView& view) -> BindlessHandle {
    const auto& desc     = view.GetDesc();
    const bool  writable = desc.type == TextureViewType::ShaderWrite;
    const auto  kind     = writable ? 2 : 1;
    auto&       device   = static_cast<VulkanDevice&>(m_Device);
    auto& [pool, mutex]  = m_BindlessHandlePools[kind];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless image heap exhausted");
    auto                             handle    = pool.back();
    const auto                       view_info = to_vk_image_view_create_info(desc, static_cast<VulkanImage&>(*desc.texture).image_handle);
    const vk::ImageDescriptorInfoEXT image_info{.pView = &view_info, .layout = writable ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::ResourceDescriptorInfoEXT    resource{.type = writable ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage};
    resource.data.pImage = &image_info;
    device.GetDevice().writeResourceDescriptorsEXT(resource, vk::HostAddressRangeEXT{
                                                                 .address = m_Heaps[0].mapped + m_Offsets[kind] + handle.index * m_DescriptorSizes[kind], .size = m_DescriptorSizes[kind]});
    pool.pop_back();
    handle.type     = BindlessHandleType::Texture;
    handle.writable = writable;
    return handle;
}

auto VulkanBindlessUtils::CreateBindlessHandle(Sampler& sampler) -> BindlessHandle {
    auto& device        = static_cast<VulkanDevice&>(m_Device);
    auto& [pool, mutex] = m_BindlessHandlePools[3];
    std::scoped_lock lock{mutex};
    if (pool.empty()) throw std::runtime_error("Bindless sampler heap exhausted");
    auto handle = pool.back();
    device.GetDevice().writeSamplerDescriptorsEXT(to_vk_sampler_create_info(sampler.GetDesc()), vk::HostAddressRangeEXT{
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
