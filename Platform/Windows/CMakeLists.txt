add_library(PlatformInterface
    WindowsApplication.cpp
)
target_link_libraries(PlatformInterface
    PUBLIC
        Common
)

add_library(MyGameEngineD3D12
    D3D12Application.cpp
    D3D12ApplicationConfig.cpp
)
target_link_libraries(MyGameEngineD3D12
    PUBLIC
        D3DRHI
        PlatformInterface
)

add_library(MyGameEngine
    OpenGLApplication.cpp
    OpenGLApplicationConfig.cpp
    ${PROJECT_SOURCE_DIR}/External/src/glad/src/glad_wgl.c
)
target_link_libraries(MyGameEngine
    PUBLIC
        OpenGLRHI
        PlatformInterface
)
