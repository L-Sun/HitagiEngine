add_library(D3DRHI
D3D12GraphicsManager.cpp
)

add_library(d3dUtil
d3dUtil.cpp
)

target_link_libraries(
    D3DRHI
    d3d12
    dxgi
    d3dcompiler
    d3dUtil
)
