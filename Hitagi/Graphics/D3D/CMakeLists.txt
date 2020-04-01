add_library(D3DRHI
    D3D12GraphicsManager.cpp
    CommandListManager.cpp
    CommandAllocatorPool.cpp
    GpuBuffer.cpp
    CommandContext.cpp
    LinearAllocator.cpp
    DescriptorAllocator.cpp
    DynamicDescriptorHeap.cpp
    RootSignature.cpp
)
add_library(d3dUtil d3dUtil.cpp)
target_link_libraries(D3DRHI
    PRIVATE
        d3d12
        dxgi
        d3dcompiler
        d3dUtil
        HitagiEngineD3D12
        GraphicsManager
)
target_include_directories(D3DRHI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
