target("editor")
    add_files("*.cpp")
    add_deps("engine")
    set_rundir("$(projectdir)")
