add_library(ShaderManager ShaderManager.cpp)
target_link_libraries(ShaderManager PUBLIC FileIOManager)

add_library(GraphicsAPI INTERFACE)
target_link_libraries(GraphicsAPI INTERFACE ShaderManager)
target_include_directories(GraphicsAPI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} "$<TARGET_PROPERTY:AssetManager,INTERFACE_INCLUDE_DIRECTORIES>")

if(WIN32)
    add_subdirectory(DX12)
endif(WIN32)

add_library(GraphicsManager
    GraphicsManager.cpp
    ResourceManager.cpp
    Frame.cpp
    FrameGraph.cpp
    PipelineState.cpp
)
target_include_directories(GraphicsManager
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        "$<TARGET_PROPERTY:Application,INTERFACE_INCLUDE_DIRECTORIES>"
)
target_link_libraries(GraphicsManager
    PUBLIC
        ShaderManager
    PRIVATE
        SceneManager
        PhysicsManager
        DebugManager
        GuiManager
        $<$<PLATFORM_ID:Windows>:DX12DriverAPI>
        freetype
)
