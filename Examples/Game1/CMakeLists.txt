add_subdirectory(MANN)

add_executable(MyGame 
    MyGame.cpp
    MyGameConfig.cpp
    DLCharacter.cpp
)
target_link_libraries(MyGame
    PRIVATE
        HitagiEngine
        HitagiPhysics
        HitagiEditor
        MANN
)

add_custom_command(TARGET MyGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    $<TARGET_RUNTIME_DLLS:MyGame>
                    $<TARGET_FILE_DIR:MyGame>
                COMMAND_EXPAND_LISTS)