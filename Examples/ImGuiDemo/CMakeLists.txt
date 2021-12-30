add_executable(ImGuiDemo ImGuiDemo.cpp ImGuiDemoConfig.cpp)
target_link_libraries(ImGuiDemo
    PRIVATE
        HitagiEngine
        HitagiPhysics
)

