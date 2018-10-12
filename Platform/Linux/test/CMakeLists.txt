add_executable(TextureLoadTest TextureLoadTest.cpp)
target_link_libraries(TextureLoadTest GLFWApplication OpenGLRHI Common glfw dl)
target_include_directories(TextureLoadTest PRIVATE "${PROJECT_SOURCE_DIR}/External/src/glad/include")
add_test(NAME TEST_TextureLoad COMMAND TextureLoadTest)