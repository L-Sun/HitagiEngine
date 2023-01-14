target("imgui-demo")
    add_files("main.cpp")
    add_deps("engine")
    set_rundir("$(projectdir)")
