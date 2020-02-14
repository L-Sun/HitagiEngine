
add_library(CommonHeader INTERFACE)
target_link_libraries(CommonHeader INTERFACE Interface)
target_include_directories(CommonHeader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(Common          BaseApplication.cpp GameLogic.cpp main.cpp)
add_library(MemoryManager   Allocator.cpp Buffer.cpp MemoryManager.cpp)
add_library(AssetLoader     AssetLoader.cpp)
add_library(SceneManager    SceneManager.cpp Scene.cpp SceneObject.cpp Image.cpp)
add_library(GraphicsManager GraphicsManager.cpp)
add_library(DebugManager    DebugManager.cpp)
add_library(InputManager    InputManager.cpp)
add_library(Timer           Timer.cpp)

target_link_libraries(MemoryManager   PUBLIC CommonHeader)
target_link_libraries(AssetLoader     PUBLIC CommonHeader MemoryManager)
target_link_libraries(Timer           PUBLIC CommonHeader)
target_link_libraries(SceneManager    PUBLIC CommonHeader Parser AssetLoader GeomMath PRIVATE crossguid)
target_link_libraries(GraphicsManager PUBLIC CommonHeader SceneManager PRIVATE freetype)
target_link_libraries(DebugManager    PUBLIC CommonHeader SceneManager)
target_link_libraries(InputManager    PUBLIC CommonHeader SceneManager)

target_include_directories(SceneManager INTERFACE "$<TARGET_PROPERTY:crossguid,INTERFACE_INCLUDE_DIRECTORIES>")

target_link_libraries(Common
    PUBLIC
        Interface
        GeomMath
        Parser
        MemoryManager
        AssetLoader
        SceneManager
        DebugManager
        InputManager
        Timer
)