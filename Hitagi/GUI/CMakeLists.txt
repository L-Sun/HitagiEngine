add_library(GuiManager GuiManager.cpp)
target_link_libraries(GuiManager
    PUBLIC
        FileIOManager
        AssetManager
        InputManager
        imgui::imgui
    PRIVATE
        spdlog::spdlog)
target_include_directories(GuiManager
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        "$<TARGET_PROPERTY:Application,INTERFACE_INCLUDE_DIRECTORIES>"
)
