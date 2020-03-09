add_library(HitagiEngineD3D12
    WindowsApplication.cpp
    WindowsApplicationConfig.cpp
)
target_link_libraries(HitagiEngineD3D12
    PUBLIC
        BaseApplication
        D3DRHI
)
target_include_directories(HitagiEngineD3D12 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(HitagiEngineOpenGL
    GLFWApplication.cpp
    OpenGLApplication.cpp
    OpenGLApplicationConfig.cpp
)
target_link_libraries(HitagiEngineOpenGL
    PUBLIC
        BaseApplication
        OpenGLRHI
    PRIVATE
        glfw
)