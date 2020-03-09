add_library(OpenGLRHI
    OpenGLGraphicsManager.cpp
)
target_link_libraries(OpenGLRHI 
    PRIVATE
        GraphicsManager
        ${OPENGL_LIBRARIES}
        glad::glad
)
target_include_directories(OpenGLRHI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})