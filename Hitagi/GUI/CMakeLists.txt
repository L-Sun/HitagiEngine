add_library(GuiManager GuiManager.cpp)
target_link_libraries(GuiManager PUBLIC Application AssetManager Timer Interface imgui::imgui spdlog::spdlog)
target_include_directories(GuiManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})