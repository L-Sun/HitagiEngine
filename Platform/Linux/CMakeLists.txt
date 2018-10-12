
add_library(GLFWApplication GLFWApplication.cpp)

add_executable(MyGameEngineOpenGL OpenGLApplication.cpp)
target_link_libraries(MyGameEngineOpenGL GLFWApplication OpenGLRHI Common glfw dl)
target_include_directories(MyGameEngineOpenGL PRIVATE "${PROJECT_SOURCE_DIR}/External/src/glad/include")
add_subdirectory(test)