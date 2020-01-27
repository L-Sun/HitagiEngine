add_library(OpenGLRHI
    OpenGLGraphicsManager.cpp
    ${PROJECT_SOURCE_DIR}/External/src/glad/src/glad.c
)
target_link_libraries(OpenGLRHI 
    PRIVATE
        Common
        ${OPENGL_LIBRARIES}
)
target_include_directories(OpenGLRHI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})