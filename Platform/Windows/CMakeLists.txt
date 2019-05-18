add_library(PlatformInterface
    WindowsApplication.cpp
)
target_link_libraries(PlatformInterface
    Common
)

add_library(MyGameEngineD3d12
    D3d12Application.cpp
    D3d12ApplicationConfig.cpp
)
target_link_libraries(MyGameEngineD3d12
    D3dRHI
    PlatformInterface
)

add_library(MyGameEngine
    OpenGLApplication.cpp
    OpenGLApplicationConfig.cpp
    ${PROJECT_SOURCE_DIR}/External/src/glad/src/glad_wgl.c
)
target_link_libraries(MyGameEngine
    OpenGLRHI
    PlatformInterface
)
