add_library(Common
    Allocator.cpp
    AssetLoader.cpp
    BaseApplication.cpp
    GraphicsManager.cpp
    MemoryManager.cpp
    Scene.cpp
    SceneObject.cpp
    SceneManager.cpp
    InputManager.cpp
    DebugManager.cpp
    GameLogic.cpp
    Timer.cpp
    main.cpp
)

target_link_libraries(Common
    PUBLIC
        Interface
        GeomMath
        Parser
)

target_include_directories(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})