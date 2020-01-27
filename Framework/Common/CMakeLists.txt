
add_library(Dependency INTERFACE)
target_link_libraries(Dependency INTERFACE Interface GeomMath)
target_include_directories(Dependency INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(Common          BaseApplication.cpp GameLogic.cpp main.cpp)
add_library(MemoryManager   Allocator.cpp MemoryManager.cpp)
add_library(AssetLoader     AssetLoader.cpp)
add_library(SceneManager    SceneManager.cpp Scene.cpp SceneObject.cpp)
add_library(GraphicsManager GraphicsManager.cpp)
add_library(DebugManager    DebugManager.cpp)
add_library(InputManager    InputManager.cpp)
add_library(Timer           Timer.cpp)
add_library(ColorSpaceConversion INTERFACE)

target_link_libraries(MemoryManager        PUBLIC    Dependency)
target_link_libraries(AssetLoader          PUBLIC    Dependency MemoryManager)
target_link_libraries(Timer                PUBLIC    Dependency)
target_link_libraries(SceneManager         PUBLIC    Dependency Parser AssetLoader PRIVATE crossguid)
target_link_libraries(GraphicsManager      PUBLIC    Dependency SceneManager)
target_link_libraries(DebugManager         PUBLIC    Dependency SceneManager)
target_link_libraries(InputManager         PUBLIC    Dependency SceneManager)
target_link_libraries(ColorSpaceConversion INTERFACE Dependency)

target_include_directories(SceneManager INTERFACE "$<TARGET_PROPERTY:crossguid,INTERFACE_INCLUDE_DIRECTORIES>")

target_link_libraries(Common
    PUBLIC
        Dependency
        Parser
        MemoryManager
        AssetLoader
        SceneManager
        DebugManager
        InputManager
        Timer
)