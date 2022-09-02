target("imgui-demo")
    add_files("main.cpp", "imgui_demo.cpp")
    add_deps("engine")
    set_rundir("$(projectdir)")
