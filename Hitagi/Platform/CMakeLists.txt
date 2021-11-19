add_library(Application main.cpp Application.cpp)
target_link_libraries(Application
    PUBLIC
        InputManager
        SceneManager
        PhysicsManager
        GraphicsManager
        DebugManager
        GuiManager
        Timer
        GameLogic
        ThreadManager
)
target_include_directories(Application INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    # Windows specific code
    add_subdirectory(Windows)
endif(WIN32)
