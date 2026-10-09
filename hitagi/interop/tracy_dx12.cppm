module;
#include <tracy/TracyD3D12.hpp>

export module interop.tracy.dx12;

export using ::TracyD3D12Ctx;
export namespace tracy {
using ::tracy::D3D12ZoneScope;
}  // namespace tracy

#ifdef TRACY_ENABLE
export namespace hitagi::interop {
auto create_tracy_context(ID3D12Device* device, ID3D12CommandQueue* queue) -> TracyD3D12Ctx;
void destroy_tracy_context(TracyD3D12Ctx context);
}  // namespace hitagi::interop

namespace hitagi::interop {
auto create_tracy_context(ID3D12Device* device, ID3D12CommandQueue* queue) -> TracyD3D12Ctx {
    return TracyD3D12Context(device, queue);
}

void destroy_tracy_context(TracyD3D12Ctx context) {
    TracyD3D12Destroy(context);
}
}  // namespace hitagi::interop
#endif
