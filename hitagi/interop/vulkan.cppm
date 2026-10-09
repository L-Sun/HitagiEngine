module;
#include <vulkan/vulkan.h>

export module interop.vulkan;

export import vulkan;

export {
using ::VkAllocationCallbacks;
using ::VkBool32;
using ::VkBuffer;
using ::VkBufferCreateInfo;
using ::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
using ::VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
using ::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
using ::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
using ::VK_SUCCESS;
using ::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
using ::VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT;
using ::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
}
export namespace hitagi::interop {
inline constexpr auto whole_size = VK_WHOLE_SIZE;
inline constexpr auto null_handle = VK_NULL_HANDLE;
}
