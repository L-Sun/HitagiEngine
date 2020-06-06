add_library(GLFWApplication GLFWApplication.cpp)
target_link_libraries(GLFWApplication PUBLIC BaseApplication PRIVATE glfw Winmm)
target_include_directories(GLFWApplication INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(HitagiEngineD3D12 D3D12Config.cpp)

target_link_libraries(HitagiEngineD3D12
    PUBLIC
        GLFWApplication
        D3DRHI
)

add_library(HitagiEngineOpenGL OpenGLConfig.cpp)
target_link_libraries(HitagiEngineOpenGL
    PUBLIC
        GLFWApplication
        OpenGLRHI
)