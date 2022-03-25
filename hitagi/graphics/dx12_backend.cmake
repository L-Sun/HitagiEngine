add_library(
    dx12_driver_api
    src/backend/dx12/driver_api.cpp
    src/backend/dx12/command_allocator_pool.cpp
    src/backend/dx12/command_list_manager.cpp
    src/backend/dx12/gpu_buffer.cpp
    src/backend/dx12/sampler.cpp
    src/backend/dx12/allocator.cpp
    src/backend/dx12/descriptor_allocator.cpp
    src/backend/dx12/command_context.cpp
    src/backend/dx12/root_signature.cpp
    src/backend/dx12/pso.cpp
    src/backend/dx12/dynamic_descriptor_heap.cpp
)
target_link_libraries(
    dx12_driver_api
    PUBLIC graphics_api
    PRIVATE math
            d3d12
            dxgi
            dxguid
)
