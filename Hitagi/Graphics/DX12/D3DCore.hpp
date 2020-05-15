#pragma once
#include "CommandListManager.hpp"
#include "DescriptorAllocator.hpp"
#include "GpuBuffer.hpp"

namespace Hitagi::Graphics::DX12 {

extern Microsoft::WRL::ComPtr<ID3D12Device6>                                 g_Device;
extern CommandListManager                                                    g_CommandManager;
extern std::array<DescriptorAllocator, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> g_DescriptorAllocator;

struct MeshInfo {
    size_t                 indexCount;
    std::vector<GpuBuffer> verticesBuffer;
    GpuBuffer              indicesBuffer;
    D3D_PRIMITIVE_TOPOLOGY primitiveType;
};

}  // namespace Hitagi::Graphics::DX12