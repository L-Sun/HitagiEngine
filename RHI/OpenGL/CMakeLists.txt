find_package(OpenGL REQUIRED)
add_library(OpenGLRHI
        OpenGLGraphicsManager.cpp
       ${PROJECT_SOURCE_DIR}/External/src/glad/src/glad.c
)
target_link_libraries(OpenGLRHI ${OPENGL_LIBRARIES})