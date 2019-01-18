find_package(OpenGL REQUIRED)

add_library(PlatformInterface
                WindowsApplication.cpp
                OpenGLApplication.cpp
                ${PROJECT_SOURCE_DIR}/External/src/glad/src/glad_wgl.c
)

target_link_libraries(PlatformInterface
                ${OPENGL_LIBRARIES}
)
target_include_directories(PlatformInterface PRIVATE 
        "${PROJECT_SOURCE_DIR}/External/src/glad/include")


add_library(MyGameEngine
                OpenGLApplicationConfig.cpp
)
target_link_libraries(MyGameEngine
                        Common 
                        OpenGLRHI 
                        PlatformInterface
)
target_include_directories(MyGameEngine PRIVATE 
        "${PROJECT_SOURCE_DIR}/External/src/glad/include")