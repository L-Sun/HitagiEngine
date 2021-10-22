add_executable(MyGame 
    MyGame.cpp
    MyGameConfig.cpp
)
target_link_libraries(MyGame
    PRIVATE
        HitagiEngine
        HitagiPhysics
)

