
add_executable(MyGameEngineOpenGL GLFWApplication.cpp OpenGLApplication.cpp)
target_link_libraries(MyGameEngineOpenGL OpenGLRHI Common glfw dl)
target_include_directories(MyGameEngineOpenGL PRIVATE "${PROJECT_SOURCE_DIR}/External/GL/include")