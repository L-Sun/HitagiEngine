add_library(ShaderManager ShaderManager.cpp)
target_link_libraries(ShaderManager PRIVATE FileIOManager)

add_library(GraphicsManager GraphicsManager.cpp)
target_include_directories(GraphicsManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GraphicsManager PUBLIC SceneManager PhysicsManager ShaderManager PRIVATE freetype)

add_subdirectory(Empty)
add_subdirectory(OpenGL)
if(${WIN32})
    add_subdirectory(D3D)
endif(${WIN32})