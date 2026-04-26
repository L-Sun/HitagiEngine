option("ispc")
    set_description("Enable or disable ispc for building math library")
    set_default(true)
    set_showmenu(true)
    add_defines("USE_ISPC")
    includes("ispc")

target("math")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("math.cppm", {public = true})
    add_deps("utils")
    set_options("ispc")
    if has_config("ispc") then
        add_deps("ispc_math")
    end
