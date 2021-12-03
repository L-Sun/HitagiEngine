add_library(GuiManager GuiManager.cpp)
target_link_libraries(GuiManager
    PUBLIC
        Application
        imgui::imgui
    PRIVATE    
        spdlog::spdlog)
target_include_directories(GuiManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})