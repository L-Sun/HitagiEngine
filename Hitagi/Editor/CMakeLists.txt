add_library(HitagiEditor Editor.cpp)
target_link_libraries(HitagiEditor
    PRIVATE
        SceneManager
        GuiManager
        spdlog::spdlog
)
target_include_directories(HitagiEditor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
