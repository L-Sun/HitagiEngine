module;
#include <vk_mem_alloc.h>

export module interop.vma;

// Preserve the C API's global-module ownership and its single implementation.
export {
    using ::VmaAllocation;
    using ::VmaAllocationCreateInfo;
    using ::VmaAllocationInfo;
    using ::VmaAllocator;
    using ::VmaAllocatorCreateInfo;
    using ::VmaTotalStatistics;
    using enum ::VmaAllocationCreateFlagBits;
    using enum ::VmaAllocatorCreateFlagBits;
    using enum ::VmaMemoryUsage;
    using ::vmaAllocateMemoryForBuffer;
    using ::vmaAllocateMemoryForImage;
    using ::vmaBindBufferMemory;
    using ::vmaBindImageMemory;
    using ::vmaCalculateStatistics;
    using ::vmaCreateAllocator;
    using ::vmaCreateBufferWithAlignment;
    using ::vmaDestroyAllocator;
    using ::vmaDestroyBuffer;
    using ::vmaFlushAllocation;
    using ::vmaFreeMemory;
    using ::vmaInvalidateAllocation;
    using ::vmaMapMemory;
    using ::vmaSetCurrentFrameIndex;
    using ::vmaUnmapMemory;
}
