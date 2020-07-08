add_library(CommandListManager CommandListManager.cpp CommandAllocatorPool.cpp)
add_library(Allocator Allocator.cpp)

add_library(DX12DriverAPI
    DX12DriverAPI.cpp
    CommandAllocatorPool.cpp
    CommandListManager.cpp
    GpuBuffer.cpp
    Sampler.cpp
    Allocator.cpp
    DescriptorAllocator.cpp
    CommandContext.cpp
    RootSignature.cpp
    PSO.cpp
    DynamicDescriptorHeap.cpp
)
target_link_libraries(DX12DriverAPI
    PRIVATE
        d3d12
        dxgi
        d3dcompiler
        HitagiMath
)
target_include_directories(DX12DriverAPI
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        "$<TARGET_PROPERTY:HitagiEngine,INTERFACE_INCLUDE_DIRECTORIES>"
)
