target("editor")
    add_files("*.cpp")
    add_deps("engine")
    add_includedirs(".")
    set_rundir("$(projectdir)")
