module;
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>

export module interop.tracy.vulkan;

export using ::TracyVkCtx;
export namespace tracy {
using ::tracy::VkCtxScope;
}  // namespace tracy

#ifdef TRACY_ENABLE
export namespace hitagi::interop {
auto create_tracy_context(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandBuffer command_buffer) -> TracyVkCtx;
void destroy_tracy_context(TracyVkCtx context);
}  // namespace hitagi::interop

namespace hitagi::interop {
auto create_tracy_context(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandBuffer command_buffer) -> TracyVkCtx {
    return TracyVkContext(physical_device, device, queue, command_buffer);
}

void destroy_tracy_context(TracyVkCtx context) {
    TracyVkDestroy(context);
}
}  // namespace hitagi::interop
#endif
