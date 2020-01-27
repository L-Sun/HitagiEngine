add_library(MemoryManager   Allocator.cpp MemoryManager.cpp)
add_library(AssetLoader     AssetLoader.cpp)
add_library(SceneManager    SceneManager.cpp Scene.cpp SceneObject.cpp)
add_library(GraphicsManager GraphicsManager.cpp)
add_library(DebugManager    DebugManager.cpp)
add_library(InputManager    InputManager.cpp)
add_library(Timer           Timer.cpp)

add_library(Dependency INTERFACE)
target_link_libraries(Dependency INTERFACE Interface)
target_include_directories(Dependency INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MemoryManager   PRIVATE Dependency)
target_link_libraries(AssetLoader     PRIVATE Dependency)
target_link_libraries(Timer           PRIVATE Dependency)
target_link_libraries(SceneManager    PRIVATE Dependency crossguid Parser)
target_link_libraries(GraphicsManager PRIVATE Dependency SceneManager)
target_link_libraries(DebugManager    PRIVATE Dependency SceneManager)
target_link_libraries(InputManager    PRIVATE Dependency SceneManager)

target_include_directories(SceneManager INTERFACE "$<TARGET_PROPERTY:crossguid,INTERFACE_INCLUDE_DIRECTORIES>")
target_include_directories(SceneManager INTERFACE "$<TARGET_PROPERTY:Parser,INTERFACE_INCLUDE_DIRECTORIES>")

add_library(Common
    BaseApplication.cpp
    GameLogic.cpp
    main.cpp
)

target_link_libraries(Common
    PUBLIC
        Interface
        GeomMath
        Parser
        MemoryManager
        AssetLoader
        SceneManager
        DebugManager
        GraphicsManager
        InputManager
        Timer
)

target_include_directories(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})